#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

namespace benchmark_utils
{
    inline constexpr Quantity kOrderQuantity = 10;

    inline void populate_same_level(OrderBook &book, std::size_t count, Side side, Price price, OrderID first_id = 1)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto id = first_id + static_cast<OrderID>(i);

            book.add_order(id, side, price, kOrderQuantity, kOrderQuantity);
        }
    }

    inline void populate_distinct_levels(OrderBook &book, std::size_t count, Side side, Price first_price, OrderID first_id = 1)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto id = first_id + static_cast<OrderID>(i);
            const auto price = first_price + static_cast<Price>(i);

            book.add_order(id, side, price, kOrderQuantity, kOrderQuantity);
        }
    }

    inline std::vector<OrderID> make_order_ids(std::size_t count, OrderID first_id = 1)
    {
        std::vector<OrderID> order_ids;
        order_ids.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
            order_ids.emplace_back(first_id + static_cast<OrderID>(i));

        return order_ids;
    }
} // namespace benchmark_utils