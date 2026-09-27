#pragma once

#include <hft/order_gateway/types.hpp>

class OrderGateway
{
public:
    OrderGateway(OrderQueue &order_queue);

    void submit_order(const OrderID id, const Side side, const Price price, const Quantity quantity);

    void cancel_order(const OrderID id);

private:
    OrderQueue &_order_queue;
};