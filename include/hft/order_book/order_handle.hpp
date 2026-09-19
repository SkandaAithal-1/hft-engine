#pragma once

#include <list>

#include <hft/order_book/types.hpp>
#include <hft/order_book/order.hpp>

struct OrderHandle
{
    Price price;
    std::list<Order>::iterator iterator;
};