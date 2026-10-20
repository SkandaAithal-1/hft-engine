#pragma once

#include <list>

#include <hft/order_book/types.hpp>
#include <hft/order_book/order.hpp>
#include <hft/order_book/price_level.hpp>

struct OrderHandle
{
    Price price;
    std::list<Order>::iterator iterator;
    std::map<Price, PriceLevel>::iterator price_level_iterator;
};