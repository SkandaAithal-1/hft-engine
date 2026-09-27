#include <hft/order_gateway/order_gateway.hpp>

OrderGateway::OrderGateway(OrderQueue &order_queue) : _order_queue(order_queue) {}

void OrderGateway::submit_order(const OrderID id, const Side side, const Price price, const Quantity quantity)
{
    _order_queue.push(
        SubmitOrderRequest{id, side, price, quantity}
    );
}

void OrderGateway::cancel_order(const OrderID id)
{
    _order_queue.push(
        CancelOrderRequest{id}
    );
}