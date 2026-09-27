#pragma once

#include <hft/order_book/types.hpp>

struct ExecutionReport 
{
    OrderID id;
    Price price;
    Quantity quantity;
};