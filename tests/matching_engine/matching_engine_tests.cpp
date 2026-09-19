#include <catch2/catch_test_macros.hpp>

#include <hft/matching_engine/matching_engine.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

TEST_CASE("Incoming Order that should cause multiple trades and gets fully filled")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    engine.process_order(1, Side::Sell, 100, 50);
    engine.process_order(2, Side::Sell, 100, 30);
    engine.process_order(3, Side::Sell, 101, 20);

    auto result = engine.process_order(4, Side::Buy, 101, 60);

    REQUIRE(trade_sink.size() == 2);
    REQUIRE(trade_sink[0].aggressor_id == 4);
    REQUIRE(trade_sink[0].resting_id == 1);
    REQUIRE(trade_sink[0].price == 100);
    REQUIRE(trade_sink[0].quantity == 50);
    REQUIRE(trade_sink[1].aggressor_id == 4);
    REQUIRE(trade_sink[1].resting_id == 2);
    REQUIRE(trade_sink[1].price == 100);
    REQUIRE(trade_sink[1].quantity == 10);

    REQUIRE(book.get_order(1) == nullptr);
    auto *order_2 = book.get_order(2);
    auto *order_3 = book.get_order(3);
    REQUIRE(order_2 != nullptr);
    REQUIRE(order_2->price == 100);
    REQUIRE(order_2->remaining == 20);
    REQUIRE(order_2->quantity == 30);
    REQUIRE(order_3 != nullptr);
    REQUIRE(order_3->price == 101);
    REQUIRE(order_3->remaining == 20);
    REQUIRE(order_3->quantity == 20);

    REQUIRE(book.get_order(4) == nullptr);
    REQUIRE(result == MatchingResult::FULLY_FILLED);
}

TEST_CASE("Resting order")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Buy, 100, 50);

    REQUIRE(result == MatchingResult::RESTING);
    auto *order = book.get_order(1);
    REQUIRE(order != nullptr);
    REQUIRE(order->price == 100);
    REQUIRE(order->quantity == 50);
    REQUIRE(order->remaining == 50);
}

TEST_CASE("Fully Filled against one resting order")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Buy, 100, 50);
    REQUIRE(result == MatchingResult::RESTING);

    result = engine.process_order(2, Side::Sell, 100, 50);
    REQUIRE(result == MatchingResult::FULLY_FILLED);
    REQUIRE(book.get_order(1) == nullptr);
    REQUIRE(book.get_order(2) == nullptr);
}

TEST_CASE("Partial fill of incoming order")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Buy, 100, 50);

    result = engine.process_order(2, Side::Sell, 100, 60);
    REQUIRE(result == MatchingResult::PARTIALLY_FILLED);
    REQUIRE(book.get_order(1) == nullptr);
    auto *order = book.get_order(2);
    REQUIRE(order->price == 100);
    REQUIRE(order->quantity == 60);
    REQUIRE(order->remaining == 10);
}

TEST_CASE("Partial fill of the resting order")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Buy, 100, 50);

    result = engine.process_order(2, Side::Sell, 100, 30);
    REQUIRE(result == MatchingResult::FULLY_FILLED);
    REQUIRE(book.get_order(2) == nullptr);
    auto *order = book.get_order(1);
    REQUIRE(order != nullptr);
    REQUIRE(order->price == 100);
    REQUIRE(order->quantity == 50);
    REQUIRE(order->remaining == 20);
}

TEST_CASE("Bid does not cross ask")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Sell, 100, 50);

    result = engine.process_order(2, Side::Buy, 99, 10);
    REQUIRE(result == MatchingResult::RESTING);
    auto *order_1 = book.get_order(1);
    auto *order_2 = book.get_order(2);
    REQUIRE(order_1 != nullptr);
    REQUIRE(order_2 != nullptr);
    REQUIRE(order_1->quantity == 50);
    REQUIRE(order_1->remaining == 50);
    REQUIRE(order_2->quantity == 10);
    REQUIRE(order_2->remaining == 10);
}

TEST_CASE("Ask does not cross bid")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Buy, 100, 50);

    result = engine.process_order(2, Side::Sell, 101, 10);
    REQUIRE(result == MatchingResult::RESTING);
    auto *order_1 = book.get_order(1);
    auto *order_2 = book.get_order(2);
    REQUIRE(order_1 != nullptr);
    REQUIRE(order_2 != nullptr);
    REQUIRE(order_1->quantity == 50);
    REQUIRE(order_1->remaining == 50);
    REQUIRE(order_2->quantity == 10);
    REQUIRE(order_2->remaining == 10);
}

TEST_CASE("Price priority works before FIFO")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Sell, 101, 20);
    result = engine.process_order(2, Side::Sell, 100, 20);
    result = engine.process_order(3, Side::Sell, 100, 20);

    result = engine.process_order(4, Side::Buy, 101, 30);

    REQUIRE(result == MatchingResult::FULLY_FILLED);
    REQUIRE(book.get_order(4) == nullptr);
    REQUIRE(book.get_order(2) == nullptr);

    auto *order_1 = book.get_order(1);
    auto *order_3 = book.get_order(3);
    REQUIRE(order_3->quantity == 20);
    REQUIRE(order_3->remaining == 10);
    REQUIRE(order_3->price == 100);
    REQUIRE(order_1->quantity == 20);
    REQUIRE(order_1->remaining == 20);
    REQUIRE(order_1->price == 101);
}

TEST_CASE("Incoming Order that should cause multiple trades with multiple price levels gets fully filled")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    engine.process_order(1, Side::Sell, 100, 20);
    engine.process_order(2, Side::Sell, 101, 20);
    engine.process_order(3, Side::Sell, 102, 20);

    auto result = engine.process_order(4, Side::Buy, 102, 50);

    REQUIRE(trade_sink.size() == 3);
    REQUIRE(trade_sink[0].aggressor_id == 4);
    REQUIRE(trade_sink[0].resting_id == 1);
    REQUIRE(trade_sink[0].price == 100);
    REQUIRE(trade_sink[0].quantity == 20);
    REQUIRE(trade_sink[1].aggressor_id == 4);
    REQUIRE(trade_sink[1].resting_id == 2);
    REQUIRE(trade_sink[1].price == 101);
    REQUIRE(trade_sink[1].quantity == 20);
    REQUIRE(trade_sink[2].aggressor_id == 4);
    REQUIRE(trade_sink[2].resting_id == 3);
    REQUIRE(trade_sink[2].price == 102);
    REQUIRE(trade_sink[2].quantity == 10);

    REQUIRE(book.get_order(1) == nullptr);
    REQUIRE(book.get_order(2) == nullptr);
    REQUIRE(book.get_order(4) == nullptr);
    auto *order_3 = book.get_order(3);
    REQUIRE(order_3 != nullptr);
    REQUIRE(order_3->price == 102);
    REQUIRE(order_3->remaining == 10);
    REQUIRE(order_3->quantity == 20);

    REQUIRE(result == MatchingResult::FULLY_FILLED);
}

TEST_CASE("Order with duplicate id returns DUPLICATE_ORDER_ID")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};

    auto result = engine.process_order(1, Side::Sell, 100, 20);
    result = engine.process_order(1, Side::Buy, 101, 10);

    REQUIRE(result == MatchingResult::DUPLICATE_ORDER_ID);
    auto *order = book.get_order(1);
    REQUIRE(order->price == 100);
    REQUIRE(order->quantity == 20);
    REQUIRE(order->remaining == 20);
}

TEST_CASE("Order with quantity=0 returns INVALID_QUANTITY")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    auto result = engine.process_order(1, Side::Buy, 100, 0);
    REQUIRE(result == MatchingResult::INVALID_QUANTITY);
}

TEST_CASE("Order with price=0 returns INVALID_PRICE")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    auto result = engine.process_order(1, Side::Buy, 0, 40);
    REQUIRE(result == MatchingResult::INVALID_PRICE);
}