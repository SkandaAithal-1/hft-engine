#include <hft/matching_engine/matching_engine.hpp>

MatchingResult MatchingEngine::process_order(OrderID id, Side side, Price price, Quantity quantity)
{
    if (price == 0)
        return MatchingResult::INVALID_PRICE;
    if (quantity == 0)
        return MatchingResult::INVALID_QUANTITY;
    if (_book.get_order(id) != nullptr)
        return MatchingResult::DUPLICATE_ORDER_ID;

    auto RETURN_STATUS = MatchingResult::RESTING;
    Quantity remaining = quantity;

    while (remaining)
    {
        auto best_order = (side == Side::Buy) ? _book.best_ask() : _book.best_bid();

        if (!best_order)
            break;

        if (side == Side::Buy && best_order->price > price)
            break;

        if (side == Side::Sell && best_order->price < price)
            break;

        const Quantity trade_quantity = std::min(remaining, best_order->remaining);
        const Price trade_price = best_order->price;
        const OrderID resting_id = best_order->id;

        auto result = _book.reduce_order(best_order->id, trade_quantity);
        _trade_sink.emplace_back(id, resting_id, trade_price, trade_quantity);
        remaining -= trade_quantity;
        RETURN_STATUS = MatchingResult::PARTIALLY_FILLED;
    }

    if (remaining == 0)
        RETURN_STATUS = MatchingResult::FULLY_FILLED;
    else
        _book.add_order(id, side, price, quantity, remaining);

    return RETURN_STATUS;
}

CancelResult MatchingEngine::cancel_order(OrderID id)
{
    return _book.cancel_order(id);
}
