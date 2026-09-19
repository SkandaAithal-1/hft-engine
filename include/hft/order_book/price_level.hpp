#pragma once

#include <list>

#include <hft/order_book/types.hpp>
#include <hft/order_book/order.hpp>

struct PriceLevel
{
    std::list<Order> orders;
    Quantity total_quantity;
};