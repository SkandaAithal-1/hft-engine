#pragma once

#include <vector>

#include <hft/order_book/types.hpp>

struct Trade
{
    OrderID aggressor_id;
    OrderID resting_id;
    Price price;
    Quantity quantity;
};

using TradeSink = std::vector<Trade>;