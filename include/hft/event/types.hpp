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

enum class OrderStatus
{
    RESTING,
    REMOVED
};

using InputEvent = std::variant<LimitOrderEvent, CancelOrderEvent>;

struct Event
{
    uint64_t sequence;
    InputEvent payload;

    const bool operator==(const Event &other) const
    {
        if (sequence != other.sequence)
            return false;

        if (payload.index() != other.payload.index())
            return false;

        if (payload.index() == 0)
        {
            const auto event1 = std::get<0>(payload);
            const auto event2 = std::get<0>(other.payload);

            return (event1.id == event2.id &&
                    event1.price == event2.price &&
                    event1.quantity == event2.quantity &&
                    event1.side == event2.side);
        }
        else
        {
            const auto event1 = std::get<1>(payload);
            const auto event2 = std::get<1>(payload);

            return (event1.id == event2.id);
        }
    }

    const bool operator!=(const Event &other) const
    {
        return !(*this == other);
    }
};