#pragma once

#include <variant>

#include <hft/order_book/types.hpp>

struct BookUpdate
{
    Price best_bid;
    Quantity bid_quantity;
    Price best_ask;
    Quantity ask_quantity;

    bool operator==(const BookUpdate &other) const
    {
        return (best_bid == other.best_bid &&
                best_ask == other.best_ask &&
                bid_quantity == other.bid_quantity &&
                ask_quantity == other.ask_quantity);
    }

    bool operator!=(const BookUpdate &other) const
    {
        return !(*this == other);
    }
};

struct TradeEvent
{
    Price price;
    Quantity quantity;
};

using MarketEvent = std::variant<BookUpdate, TradeEvent>;