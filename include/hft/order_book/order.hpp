#pragma once

#include <cstdint>

#include <hft/order_book/types.hpp>

struct Order
{
    OrderID id;
    Side side;
    Price price;
    Quantity quantity;
    Quantity remaining;
    std::uint64_t sequence;
};