#include <catch2/catch_test_macros.hpp>

#include <hft/event/event_processor.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/matching_engine/matching_engine.hpp>

TEST_CASE("Limit order is forwarded correctly")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor processor{engine};

    const Event event{0, LimitOrderEvent{0, Side::Buy, 100, 10}};
    auto result = processor.process(event);

    REQUIRE(result.index() == 0);
    REQUIRE(std::get<MatchingResult>(result) == MatchingResult::RESTING);
}

TEST_CASE("Cancel order is forwarded correctly")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor processor{engine};

    const Event event_1{0, LimitOrderEvent{0, Side::Buy, 100, 10}};
    const Event event_2{1, CancelOrderEvent{0}};
    auto result = processor.process(event_1);
    result = processor.process(event_2);

    REQUIRE(result.index() == 1);
    REQUIRE(std::get<CancelResult>(result) == CancelResult::SUCCESS);
}

TEST_CASE("Trade events are correctly propogated")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor processor{engine};

    const Event event_1{0, LimitOrderEvent{0, Side::Buy, 100, 10}};
    const Event event_2{1, LimitOrderEvent{1, Side::Sell, 99, 5}};
    auto result = processor.process(event_1);
    result = processor.process(event_2);

    REQUIRE(result.index() == 0);
    REQUIRE(std::get<MatchingResult>(result) == MatchingResult::FULLY_FILLED);
    REQUIRE(book.get_order(0) != nullptr);
    REQUIRE(book.get_order(0)->remaining == 5);
    REQUIRE(book.get_order(0)->quantity == 10);
    REQUIRE(trade_sink.size() == 1);
    REQUIRE(trade_sink[0].aggressor_id == 1);
    REQUIRE(trade_sink[0].resting_id == 0);
    REQUIRE(trade_sink[0].price == 100);
    REQUIRE(trade_sink[0].quantity == 5);
}