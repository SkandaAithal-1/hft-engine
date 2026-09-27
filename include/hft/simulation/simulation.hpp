#pragma once

#include <unordered_map>

#include <hft/event/event_source.hpp>
#include <hft/event/event_processor.hpp>
#include <hft/matching_engine/matching_engine.hpp>
#include <hft/strategy/strategy.hpp>
#include <hft/order_gateway/order_gateway.hpp>

class Simulation
{
public:
    Simulation(
        OrderBook &order_book,
        EventSource &event_source,
        EventProcessor &event_processor,
        MatchingEngine &matching_engine,
        Strategy &strategy,
        TradeSink &trade_sink,
        OrderQueue &order_queue);

    void run();

    void process_order_queue();

    void publish_market_data();

private:
    OrderBook &_order_book;
    EventSource &_event_source;
    EventProcessor &_event_processor;
    MatchingEngine &_matching_engine;
    Strategy &_strategy;
    TradeSink &_trade_sink;
    OrderQueue &_order_queue;

    std::unordered_map<OrderID, Quantity> _strategy_orders;
    std::uint64_t processed_trade_count{};
    BookUpdate last_published_update{};
};