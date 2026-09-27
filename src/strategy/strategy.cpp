#include <hft/strategy/strategy.hpp>

Strategy::Strategy(OrderGateway &order_gateway) : _gateway(order_gateway)
{
}