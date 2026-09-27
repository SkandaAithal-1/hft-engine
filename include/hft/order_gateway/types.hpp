#pragma once

#include <variant>
#include <queue>

#include <hft/order_book/types.hpp>

struct SubmitOrderRequest
{
    OrderID id;
    Side side;
    Price price;
    Quantity quantity;
};

struct CancelOrderRequest
{
    OrderID id;
};

using OrderRequest = std::variant<SubmitOrderRequest, CancelOrderRequest>;
using OrderQueue = std::queue<OrderRequest>;