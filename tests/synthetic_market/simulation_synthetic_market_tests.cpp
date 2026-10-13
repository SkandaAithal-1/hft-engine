#include <catch2/catch_test_macros.hpp>

#include <hft/event/event_processor.hpp>
#include <hft/synthetic_market/synthetic_market.hpp>
#include <hft/matching_engine/matching_engine.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/order_gateway/order_gateway.hpp>
#include <hft/simulation/simulation.hpp>
#include <hft/strategy/strategy.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace
{

    SyntheticMarketConfig make_config(
        std::size_t event_count,
        double buy_probability = 1.0,
        double cancel_probability = 0.0)
    {
        return SyntheticMarketConfig{
            .seed = 42,
            .event_count = event_count,
            .reference_price = 100,
            .min_price_offset = 0,
            .max_price_offset = 0,
            .min_quantity = 5,
            .max_quantity = 5,
            .buy_probability = buy_probability,
            .cancel_probability = cancel_probability,
        };
    }

    class RecordingStrategy : public Strategy
    {
    public:
        explicit RecordingStrategy(OrderGateway &gateway)
            : Strategy(gateway)
        {
        }

        void on_execution(const ExecutionReport &report) override
        {
            executions.push_back(report);
        }

        void on_market_data(const MarketEvent &event) override
        {
            market_events.push_back(event);
        }

        std::vector<ExecutionReport> executions;
        std::vector<MarketEvent> market_events;
    };

    class CrossingStrategy : public RecordingStrategy
    {
    public:
        explicit CrossingStrategy(OrderGateway &gateway)
            : RecordingStrategy(gateway)
        {
        }

        void on_market_data(const MarketEvent &event) override
        {
            RecordingStrategy::on_market_data(event);

            if (submitted)
                return;

            const auto *update = std::get_if<BookUpdate>(&event);
            if (update == nullptr || update->best_bid == 0)
                return;

            submitted = true;
            _gateway.submit_order(9000, Side::Sell, 100, 5);
        }

    private:
        bool submitted = false;
    };

} // namespace

TEST_CASE("Simulation processes a finite synthetic market")
{
    OrderBook book;
    TradeSink trade_sink;
    OrderQueue order_queue;

    MatchingEngine engine{book, trade_sink};
    EventProcessor processor{engine};
    OrderGateway gateway{order_queue};
    RecordingStrategy strategy{gateway};

    SyntheticMarket market{make_config(3)};

    Simulation simulation{
        book,
        market,
        processor,
        engine,
        strategy,
        trade_sink,
        order_queue};

    simulation.run();

    // All three generated orders are buys at price 100.
    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_bid()->price == 100);
    REQUIRE(book.quantity_at(Side::Buy, 100) == 15);

    // No generated order crosses another generated order.
    REQUIRE(trade_sink.empty());
}

TEST_CASE("Synthetic market cancellation removes a resting order")
{
    OrderBook book;
    TradeSink trade_sink;
    OrderQueue order_queue;

    MatchingEngine engine{book, trade_sink};
    EventProcessor processor{engine};
    OrderGateway gateway{order_queue};
    RecordingStrategy strategy{gateway};

    // With no active orders, the first event must be a limit order.
    // Once it rests, cancel_probability = 1 forces a cancellation.
    SyntheticMarket market{make_config(2, 1.0, 1.0)};

    Simulation simulation{
        book,
        market,
        processor,
        engine,
        strategy,
        trade_sink,
        order_queue};

    simulation.run();

    REQUIRE_FALSE(book.best_bid().has_value());
    REQUIRE_FALSE(book.best_ask().has_value());
    REQUIRE(trade_sink.empty());
}

TEST_CASE("Strategy can execute against a synthetic market order")
{
    OrderBook book;
    TradeSink trade_sink;
    OrderQueue order_queue;

    MatchingEngine engine{book, trade_sink};
    EventProcessor processor{engine};
    OrderGateway gateway{order_queue};
    CrossingStrategy strategy{gateway};

    // The source generates exactly one buy order:
    // Buy 5 @ 100.
    SyntheticMarket market{make_config(1)};

    Simulation simulation{
        book,
        market,
        processor,
        engine,
        strategy,
        trade_sink,
        order_queue};

    simulation.run();

    // The strategy reacts to the published bid by submitting
    // a sell order at 100, which crosses the synthetic buy.
    REQUIRE(trade_sink.size() == 1);

    REQUIRE(trade_sink[0].aggressor_id == 9000);
    REQUIRE(trade_sink[0].resting_id == 0);
    REQUIRE(trade_sink[0].price == 100);
    REQUIRE(trade_sink[0].quantity == 5);

    // The strategy receives its execution report.
    REQUIRE(strategy.executions.size() == 1);
    REQUIRE(strategy.executions[0].id == 9000);
    REQUIRE(strategy.executions[0].price == 100);
    REQUIRE(strategy.executions[0].quantity == 5);

    // Both orders were fully filled.
    REQUIRE_FALSE(book.best_bid().has_value());
    REQUIRE_FALSE(book.best_ask().has_value());
}