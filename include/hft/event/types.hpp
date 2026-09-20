#pragma once

#include <variant>

#include <hft/order_book/types.hpp>

struct LimitOrderEvent
{
    OrderID id;
    Side side;
    Price price;
    Quantity quantity;
};

struct CancelOrderEvent
{
    OrderID id;
};

using InputEvent = std::variant<LimitOrderEvent, CancelOrderEvent>;

struct Event
{
    uint64_t sequence;
    InputEvent payload;
};