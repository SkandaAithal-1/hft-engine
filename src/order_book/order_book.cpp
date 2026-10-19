#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

OrderBook::OrderBook(const std::size_t expected_order_count)
{
    if (expected_order_count > 0)
        _order_index.reserve(expected_order_count);
}

AddResult OrderBook::add_order(const OrderID id, const Side side, const Price price, const Quantity quantity, const Quantity remaining)
{
    if (quantity == 0)
        return AddResult::INVALID_QUANTITY;

    if (price == 0)
        return AddResult::INVALID_PRICE;

    if (_order_index.find(id) != _order_index.end())
        return AddResult::DUPLICATE_ORDER_ID;

    auto &book_side = (side == Side::Buy)
                          ? _bids
                          : _asks;

    if (book_side.find(price) == book_side.end())
        book_side[price] = PriceLevel{};

    book_side[price].orders.emplace_back(id, side, price, quantity, remaining, ++sequence_counter);
    book_side[price].total_quantity += quantity;
    _order_index[id] = OrderHandle{price, std::prev(book_side[price].orders.end())};

    return AddResult::SUCCESS;
}

CancelResult OrderBook::cancel_order(const OrderID id)
{
    auto it = _order_index.find(id);
    if (it == _order_index.end())
        return CancelResult::NOT_FOUND;

    const OrderHandle handle = it->second;
    const Price price = handle.price;
    const Side side = handle.iterator->side;
    auto &book_side = (side == Side::Buy)
                          ? _bids
                          : _asks;
    auto price_level_it = book_side.find(price);

    book_side[price].total_quantity -= handle.iterator->remaining;
    book_side[price].orders.erase(handle.iterator);

    if (book_side[price].orders.empty())
        book_side.erase(price_level_it);

    _order_index.erase(it);
    return CancelResult::SUCCESS;
}

std::optional<Order> OrderBook::best_bid() const
{
    if (_bids.empty())
        return std::nullopt;

    return _bids.rbegin()->second.orders.front();
}

std::optional<Order> OrderBook::best_ask() const
{
    if (_asks.empty())
        return std::nullopt;

    return _asks.begin()->second.orders.front();
}

Quantity OrderBook::quantity_at(const Side side, const Price price) const
{
    auto &book_side = (side == Side::Buy)
                          ? _bids
                          : _asks;

    auto it = book_side.find(price);
    if (it != book_side.end())
        return it->second.total_quantity;

    return 0;
}

std::size_t OrderBook::order_count_at(const Side side, const Price price) const
{
    auto &book_side = (side == Side::Buy)
                          ? _bids
                          : _asks;

    auto it = book_side.find(price);
    if (it != book_side.end())
        return it->second.orders.size();

    return static_cast<std::size_t>(0);
}

const Order *OrderBook::get_order(const OrderID id) const
{
    auto it = _order_index.find(id);
    if (it == _order_index.end())
        return nullptr;

    return &(*(it->second.iterator));
}

ReduceResult OrderBook::reduce_order(const OrderID id, const Quantity quantity)
{
    auto it = _order_index.find(id);
    if (it == _order_index.end())
        return ReduceResult::NOT_FOUND;

    auto order_it = it->second.iterator;

    if (quantity == 0 || quantity > order_it->remaining)
        return ReduceResult::INVALID_QUANTITY;

    order_it->remaining -= quantity;

    auto &side = (order_it->side == Side::Buy)
                     ? _bids
                     : _asks;

    auto price_level_it = side.find(order_it->price);
    price_level_it->second.total_quantity -= quantity;

    if (order_it->remaining == 0)
    {
        price_level_it->second.orders.erase(order_it);
        _order_index.erase(it);

        if (price_level_it->second.orders.empty())
            side.erase(price_level_it);
    }

    return ReduceResult::REDUCED;
}