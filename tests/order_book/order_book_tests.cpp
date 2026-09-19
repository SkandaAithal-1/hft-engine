#include <catch2/catch_test_macros.hpp>

#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

// Add
TEST_CASE("Adds a valid order")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 100;
    const Quantity quantity = 10;

    const auto result = book.add_order(id, side, price, quantity, quantity);

    REQUIRE(result == AddResult::SUCCESS);
}

TEST_CASE("Adding an order with a duplicate ID returns DUPLICATE_ORDER_ID")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 100;
    const Quantity quantity = 10;

    book.add_order(id, side, price, quantity, quantity);

    const auto result = book.add_order(id, side, price, quantity, quantity);

    REQUIRE(result == AddResult::DUPLICATE_ORDER_ID);
}

TEST_CASE("Adding an order with a price of 0 returns INVALID_PRICE")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 0;
    const Quantity quantity = 10;

    const auto result = book.add_order(id, side, price, quantity, quantity);

    REQUIRE(result == AddResult::INVALID_PRICE);
    REQUIRE(book.get_order(id) == nullptr);
    REQUIRE(book.quantity_at(side, price) == 0);
}

TEST_CASE("Adding an order with a quantity of 0 returns INVALID_QUANTITY")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 100;
    const Quantity quantity = 0;

    const auto result = book.add_order(id, side, price, quantity, quantity);

    REQUIRE(result == AddResult::INVALID_QUANTITY);
    REQUIRE(book.get_order(id) == nullptr);
    REQUIRE(book.quantity_at(side, price) == 0);
}

// Cancel
TEST_CASE("Cancels and existing order")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 100;
    const Quantity quantity = 10;

    book.add_order(id, side, price, quantity, quantity);
    REQUIRE(book.get_order(id) != nullptr);

    const auto cancel_result = book.cancel_order(id);

    REQUIRE(cancel_result == CancelResult::SUCCESS);
    REQUIRE(book.get_order(id) == nullptr);
    REQUIRE(book.quantity_at(side, price) == 0);
    REQUIRE(book.order_count_at(side, price) == 0);
}

TEST_CASE("Cancelling a non-existing order returns NOT_FOUND")
{
    OrderBook book;

    const OrderID id = 1;

    const auto cancel_result = book.cancel_order(id);

    REQUIRE(cancel_result == CancelResult::NOT_FOUND);
}

TEST_CASE("Cancel removes empty price level")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 100;
    const Quantity quantity = 10;

    book.add_order(id, side, price, quantity, quantity);

    // Cancel the order
    book.cancel_order(id);

    // Check that the price level is removed
    REQUIRE(book.quantity_at(side, price) == 0);
    REQUIRE(book.order_count_at(side, price) == 0);
}

TEST_CASE("Multiple price levels correctly collapse")
{
    OrderBook book;

    const OrderID id1 = 1, id2 = 2, id3 = 3, id4 = 4;
    const Side side = Side::Buy;
    const Price price1 = 100, price2 = 101, price3 = 102;
    const Quantity quantity1 = 5, quantity2 = 10, quantity3 = 15, quantity4 = 20;

    book.add_order(id1, side, price1, quantity1, quantity1);
    book.add_order(id2, side, price2, quantity2, quantity2);
    book.add_order(id4, side, price2, quantity4, quantity4);
    book.add_order(id3, side, price3, quantity3, quantity3);

    // Verify the best bid is price3
    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_bid()->price == price3);

    // Cancel id3
    book.cancel_order(id3);

    // Verify that the best bid is now price2
    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_bid()->price == price2);

    // Cancel id2
    book.cancel_order(id2);

    // Verify that the best bid is still price2 (since id4 is still there)
    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_bid()->price == price2);

    // Cancel id4
    book.cancel_order(id4);

    // Verify that the best bid is now price1
    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_bid()->price == price1);

    // Cancel id1
    book.cancel_order(id1);

    // Verify that there are no more bids
    REQUIRE(!book.best_bid().has_value());
}

// Price-time priority
TEST_CASE("FIFO order is maintained at the same price level")
{
    OrderBook book;

    const Price price = 100;
    const Quantity quantity1 = 10;
    const Quantity quantity2 = 5;

    // Add two orders at the same price
    book.add_order(1, Side::Buy, price, quantity1, quantity1);
    book.add_order(2, Side::Buy, price, quantity2, quantity2);

    // Check that the best bid is the first order added
    const auto best_bid = book.best_bid();
    REQUIRE(best_bid.has_value());
    REQUIRE(best_bid->id == 1);
    REQUIRE(best_bid->quantity == quantity1);
}

TEST_CASE("Best bid and best ask are returned correctly")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 10, 10);
    book.add_order(2, Side::Buy, 101, 5, 5);
    book.add_order(3, Side::Sell, 102, 8, 8);
    book.add_order(4, Side::Sell, 103, 12, 12);

    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();

    REQUIRE(best_bid.has_value());
    REQUIRE(best_bid->price == 101);
    REQUIRE(best_bid->quantity == 5);

    REQUIRE(best_ask.has_value());
    REQUIRE(best_ask->price == 102);
    REQUIRE(best_ask->quantity == 8);
}

TEST_CASE("Best bid and best ask return std::nullopt when no orders exist")
{
    OrderBook book;

    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();

    REQUIRE(!best_bid.has_value());
    REQUIRE(!best_ask.has_value());
}

// Quantity and order count at a specific price
TEST_CASE("Quantity at a specific price is returned correctly")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 10, 10);
    book.add_order(2, Side::Buy, 100, 5, 5);
    book.add_order(3, Side::Sell, 102, 8, 8);
    book.add_order(4, Side::Sell, 102, 12, 12);

    const auto buy_quantity = book.quantity_at(Side::Buy, 100);
    const auto sell_quantity = book.quantity_at(Side::Sell, 102);

    REQUIRE(buy_quantity == 15);
    REQUIRE(sell_quantity == 20);
}

TEST_CASE("Order count at a specific price is returned correctly")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 10, 10);
    book.add_order(2, Side::Buy, 100, 5, 5);
    book.add_order(3, Side::Sell, 102, 8, 8);
    book.add_order(4, Side::Sell, 102, 12, 12);

    const auto buy_count = book.order_count_at(Side::Buy, 100);
    const auto sell_count = book.order_count_at(Side::Sell, 102);

    REQUIRE(buy_count == 2);
    REQUIRE(sell_count == 2);
}

TEST_CASE("Quantity and order count at a specific price return 0 when no orders exist")
{
    OrderBook book;

    const auto buy_quantity = book.quantity_at(Side::Buy, 100);
    const auto sell_quantity = book.quantity_at(Side::Sell, 102);
    const auto buy_count = book.order_count_at(Side::Buy, 100);
    const auto sell_count = book.order_count_at(Side::Sell, 102);

    REQUIRE(buy_quantity == 0);
    REQUIRE(sell_quantity == 0);
    REQUIRE(buy_count == 0);
    REQUIRE(sell_count == 0);
}

// Order Lookup
TEST_CASE("Get order by ID returns the correct order")
{
    OrderBook book;

    const OrderID id = 1;
    const Side side = Side::Buy;
    const Price price = 100;
    const Quantity quantity = 10;

    book.add_order(id, side, price, quantity, quantity);

    const auto order_ptr = book.get_order(id);

    REQUIRE(order_ptr != nullptr);
    REQUIRE(order_ptr->id == id);
    REQUIRE(order_ptr->side == side);
    REQUIRE(order_ptr->price == price);
    REQUIRE(order_ptr->quantity == quantity);
    REQUIRE(order_ptr->remaining == quantity);
}

TEST_CASE("Get order by ID returns nullptr for non-existing order")
{
    OrderBook book;

    const auto order_ptr = book.get_order(1);

    REQUIRE(order_ptr == nullptr);
}

// Reduce
TEST_CASE("Partial Reduction succeeds")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 20, 20);
    auto result = book.reduce_order(1, 10);

    REQUIRE(result == ReduceResult::REDUCED);
    auto *order = book.get_order(1);
    REQUIRE(order->quantity == 20);
    REQUIRE(order->remaining == 10);
}

TEST_CASE("Full Reduction succeeds and order is removed")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 20, 20);
    auto result = book.reduce_order(1, 20);

    REQUIRE(result == ReduceResult::REDUCED);
    REQUIRE(book.get_order(1) == nullptr);
}

TEST_CASE("Reduction of a non-existent order returns NOT_FOUND")
{
    OrderBook book;

    auto result = book.reduce_order(10, 40);

    REQUIRE(result == ReduceResult::NOT_FOUND);
}

TEST_CASE("Reduction quantity larger than remaining returns INVALID_QUANTITY")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 20, 20);
    auto result = book.reduce_order(1, 40);

    REQUIRE(result == ReduceResult::INVALID_QUANTITY);
    auto *order = book.get_order(1);
    REQUIRE(order != nullptr);
    REQUIRE(order->quantity == 20);
    REQUIRE(order->remaining == 20);
}

TEST_CASE("Reduction quantity=0 returns INVALID_QUANTITY")
{
    OrderBook book;

    book.add_order(1, Side::Buy, 100, 20, 20);
    auto result = book.reduce_order(1, 0);

    REQUIRE(result == ReduceResult::INVALID_QUANTITY);
    auto *order = book.get_order(1);
    REQUIRE(order != nullptr);
    REQUIRE(order->quantity == 20);
    REQUIRE(order->remaining == 20);
}