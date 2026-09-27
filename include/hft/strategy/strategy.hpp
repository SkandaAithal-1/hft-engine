#pragma once

#include <hft/strategy/types.hpp>
#include <hft/market_data/types.hpp>
#include <hft/order_gateway/order_gateway.hpp>

class Strategy
{
public:
    explicit Strategy(OrderGateway &gateway);
    virtual ~Strategy() = default;

    virtual void on_execution(const ExecutionReport &report) = 0;

    virtual void on_market_data(const MarketEvent &event) = 0;

protected:
    OrderGateway &_gateway;
};