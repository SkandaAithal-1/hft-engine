#pragma once

#include <hft/matching_engine/trade.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

class MatchingEngine
{
public:
    explicit MatchingEngine(OrderBook &book, TradeSink &trade_sink) : _book(book), _trade_sink(trade_sink) {}

    MatchingResult process_order(OrderID id, Side side, Price price, Quantity quantity);

    CancelResult cancel_order(OrderID id);

private:
    OrderBook &_book;
    TradeSink &_trade_sink;
};