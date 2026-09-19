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
    OrderBook();

    AddResult add_order(OrderID id, Side side, Price price, Quantity quantity, Quantity remaining);

    CancelResult cancel_order(OrderID id);

    std::optional<Order> best_bid() const;

    std::optional<Order> best_ask() const;

    Quantity quantity_at(Side side, Price price) const;

    std::size_t order_count_at(Side side, Price price) const;

    const Order *get_order(OrderID id) const;

    ReduceResult reduce_order(OrderID id, Quantity quantity);

private:
    std::uint64_t sequence_counter;
    std::map<Price, PriceLevel> _bids;
    std::map<Price, PriceLevel> _asks;
    std::unordered_map<OrderID, OrderHandle> _order_index;
};