#pragma once

#include <map>
#include <optional>
#include <unordered_map>

#include <hft/order_book/price_level.hpp>
#include <hft/order_book/order.hpp>
#include <hft/order_book/order_handle.hpp>

class OrderBook
{
public:
    explicit OrderBook(std::size_t expected_order_count = 0);

    AddResult add_order(const OrderID id, const Side side, const Price price, const Quantity quantity, const Quantity remaining);

    CancelResult cancel_order(const OrderID id);

    std::optional<Order> best_bid() const;

    std::optional<Order> best_ask() const;

    std::optional<BestLevel> best_bid_price_with_quantity() const;

    std::optional<BestLevel> best_ask_price_with_quantity() const;

    Quantity quantity_at(const Side side, const Price price) const;

    std::size_t order_count_at(const Side side, const Price price) const;

    const Order *get_order(const OrderID id) const;

    ReduceResult reduce_order(const OrderID id, const Quantity quantity);

private:
    std::uint64_t sequence_counter;
    std::map<Price, PriceLevel> _bids;
    std::map<Price, PriceLevel> _asks;
    std::unordered_map<OrderID, OrderHandle> _order_index;
};