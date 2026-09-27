#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <random>

#include <hft/event/event_source.hpp>
#include <hft/event/types.hpp>
#include <hft/order_book/types.hpp>

struct SyntheticMarketConfig
{
    std::uint64_t seed;
    std::size_t event_count;

    Price reference_price;
    Price min_price_offset;
    Price max_price_offset;

    Quantity min_quantity;
    Quantity max_quantity;

    double buy_probability;
    double cancel_probability;
};

class SyntheticMarket : public EventSource
{
public:
    explicit SyntheticMarket(const SyntheticMarketConfig &config);

    bool next(Event &event) override;

    void on_order_update(const OrderID id, const OrderStatus status) override;

private:
    SyntheticMarketConfig _config;
    std::uint64_t _next_event_sequence{};
    OrderID _next_order_id{};

    std::mt19937_64 _rng;
    std::unordered_map<OrderID, std::size_t> _order_indices;
    std::vector<OrderID> _active_orders;

    void add_active_order(const OrderID id);
    void remove_active_order(const OrderID id);
};