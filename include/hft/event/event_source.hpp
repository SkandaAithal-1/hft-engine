#pragma once

#include <hft/event/types.hpp>

class EventSource
{
public:
    virtual ~EventSource() = default;

    virtual bool next(Event &event) = 0;

    virtual void on_order_update(const OrderID id, const OrderStatus status) {}
};