#include <hft/synthetic_market/synthetic_market.hpp>

SyntheticMarket::SyntheticMarket(const SyntheticMarketConfig &config) : _config(config), _rng(config.seed)
{
    assert(_config.min_price_offset <= _config.max_price_offset);
    assert(_config.min_quantity <= _config.max_quantity);
    assert(0 <= _config.buy_probability && _config.buy_probability <= 1);
    assert(0 <= _config.cancel_probability && _config.cancel_probability <= 1);
}

bool SyntheticMarket::next(Event &event)
{
    if (_next_event_sequence >= _config.event_count)
        return false;

    std::uniform_real_distribution<double> event_distribution{0.0, 1.0};
    const bool is_cancel = !_active_orders.empty() && std::bernoulli_distribution{_config.cancel_probability}(_rng);

    const std::uint64_t sequence = _next_event_sequence++;

    if (is_cancel)
    {
        std::uniform_int_distribution<std::size_t> index_distribution{0, _active_orders.size() - 1};

        const std::size_t index = index_distribution(_rng);
        const OrderID id = _active_orders[index];

        event = Event{sequence, CancelOrderEvent{id}};
        return true;
    }

    const bool is_buy = std::bernoulli_distribution{_config.buy_probability}(_rng);
    std::uniform_int_distribution<Price> price_distribution{_config.min_price_offset, _config.max_price_offset};
    std::uniform_int_distribution<Price> quantity_distribution{_config.min_quantity, _config.max_quantity};

    const Price price = _config.reference_price + price_distribution(_rng);
    const Quantity quantity = quantity_distribution(_rng);
    const OrderID id = _next_order_id++;

    event = Event{
        sequence,
        LimitOrderEvent{id, is_buy ? Side::Buy : Side::Sell, price, quantity}};

    return true;
}

void SyntheticMarket::add_active_order(const OrderID id)
{
    const std::size_t index = _active_orders.size();

    const auto [_, inserted] = _order_indices.emplace(id, index);

    assert(inserted);

    _active_orders.push_back(id);
}

void SyntheticMarket::remove_active_order(const OrderID id)
{
    const std::size_t index = _order_indices[id];

    const OrderID last_id = _active_orders.back();

    _active_orders[index] = last_id;
    _order_indices[last_id] = index;

    _active_orders.pop_back();
    _order_indices.erase(id);
}

void SyntheticMarket::on_order_update(const OrderID id, const OrderStatus status)
{
    switch (status)
    {
    case OrderStatus::RESTING:
        add_active_order(id);
        break;
    case OrderStatus::REMOVED:
        remove_active_order(id);
        break;
    }
}