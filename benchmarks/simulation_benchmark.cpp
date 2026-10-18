
#include <benchmark/benchmark.h>

#include <cstdint>
#include <variant>

#include <hft/event/event_processor.hpp>
#include <hft/matching_engine/matching_engine.hpp>
#include <hft/market_data/types.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/order_gateway/order_gateway.hpp>
#include <hft/simulation/simulation.hpp>
#include <hft/strategy/strategy.hpp>
#include <hft/synthetic_market/synthetic_market.hpp>
#include <hft/matching_engine/trade.hpp>

namespace
{
    constexpr Price kReferencePrice = 100'000;
    constexpr Price kMaxPriceOffset = 1'000;
    constexpr Quantity kStrategyOrderQuantity = 10;
    constexpr OrderID kFirstStrategyOrderId = 1'000'000'000;

    SyntheticMarketConfig make_config(const std::size_t event_count)
    {
        return SyntheticMarketConfig{
            .seed = 42,
            .event_count = event_count,
            .reference_price = kReferencePrice,
            .min_price_offset = 1,
            .max_price_offset = kMaxPriceOffset,
            .min_quantity = 1,
            .max_quantity = 100,
            .buy_probability = 0.5,
            .cancel_probability = 0.2,
        };
    }

    class PassiveStrategy final : public Strategy
    {
    public:
        using Strategy::Strategy;

        void on_execution(const ExecutionReport &) override {}

        void on_market_data(const MarketEvent &) override {}
    };

    // Performs at most one initial submission and one cancel/replace cycle.
    // The finite action count prevents feedback loops caused by the simulation
    // publishing book updates after processing strategy requests.
    class ActiveStrategy final : public Strategy
    {
    public:
        using Strategy::Strategy;

        void on_execution(const ExecutionReport &report) override
        {
            if (report.id != _active_order_id)
                return;

            if (report.quantity >= _remaining_quantity)
            {
                _active_order_id = 0;
                _remaining_quantity = 0;
            }
            else
            {
                _remaining_quantity -= report.quantity;
            }
        }

        void on_market_data(const MarketEvent &event) override
        {
            const auto *book = std::get_if<BookUpdate>(&event);
            if (book == nullptr || _finished)
                return;

            if (!_started)
            {
                _started = true;
                submit_replacement(*book);
                return;
            }

            if (!_replaced && _active_order_id != 0)
            {
                _gateway.cancel_order(_active_order_id);
                _active_order_id = 0;
                _remaining_quantity = 0;

                _replaced = true;
                submit_replacement(*book);
                _finished = true;
            }
        }

    private:
        OrderID _active_order_id = 0;
        Quantity _remaining_quantity = 0;
        OrderID _next_order_id = kFirstStrategyOrderId;

        bool _started = false;
        bool _replaced = false;
        bool _finished = false;

        void submit_replacement(const BookUpdate &book)
        {
            const Side side = (_next_order_id % 2 == 0) ? Side::Buy : Side::Sell;

            Price price;
            if (side == Side::Buy)
            {
                price = book.best_bid != 0 ? book.best_bid : kReferencePrice;
            }
            else
            {
                price = book.best_ask != 0
                            ? book.best_ask
                            : kReferencePrice + kMaxPriceOffset + 1;
            }

            _active_order_id = _next_order_id++;
            _remaining_quantity = kStrategyOrderQuantity;

            _gateway.submit_order(
                _active_order_id,
                side,
                price,
                kStrategyOrderQuantity
            );
        }
    };

    template <typename StrategyType>
    void run_simulation_benchmark(benchmark::State &state)
    {
        const auto event_count = static_cast<std::size_t>(state.range(0));

        for (auto _ : state)
        {
            // Exclude construction and destruction from the measured interval.
            state.PauseTiming();
            {
                OrderBook order_book;
                TradeSink trade_sink;

                MatchingEngine matching_engine(order_book, trade_sink);
                EventProcessor event_processor(matching_engine);

                SyntheticMarket event_source(make_config(event_count));

                OrderQueue order_queue;
                OrderGateway gateway(order_queue);
                StrategyType strategy(gateway);

                Simulation simulation(
                    order_book,
                    event_source,
                    event_processor,
                    matching_engine,
                    strategy,
                    trade_sink,
                    order_queue);

                state.ResumeTiming();
                simulation.run();
                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        // Throughput is reported in input events per second.
        state.SetItemsProcessed(
            state.iterations() * static_cast<std::int64_t>(event_count));
    }

    void BM_Simulation_Passive(benchmark::State &state)
    {
        run_simulation_benchmark<PassiveStrategy>(state);
    }

    void BM_Simulation_Active(benchmark::State &state)
    {
        run_simulation_benchmark<ActiveStrategy>(state);
    }

    BENCHMARK(BM_Simulation_Passive)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(1'000'000);

    BENCHMARK(BM_Simulation_Active)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(1'000'000);

} // namespace

BENCHMARK_MAIN();
