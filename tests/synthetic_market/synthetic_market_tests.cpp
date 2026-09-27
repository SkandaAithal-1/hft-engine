#include <catch2/catch_test_macros.hpp>

#include <hft/synthetic_market/synthetic_market.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

    SyntheticMarketConfig make_config()
    {
        return SyntheticMarketConfig{
            .seed = 12345,
            .event_count = 100,
            .reference_price = 1000,
            .min_price_offset = -10,
            .max_price_offset = 10,
            .min_quantity = 10,
            .max_quantity = 100,
            .buy_probability = 0.5,
            .cancel_probability = 0.5};
    }

    std::vector<Event> generate_events(
        const SyntheticMarketConfig &config)
    {
        SyntheticMarket market{config};

        std::vector<Event> events;
        Event event{};

        while (market.next(event))
            events.push_back(event);

        return events;
    }

} // namespace

TEST_CASE("Synthetic market generates configured number of events")
{
    auto config = make_config();
    config.event_count = 100;

    SyntheticMarket market{config};

    Event event{};
    std::size_t count{};

    while (market.next(event))
        ++count;

    REQUIRE(count == 100);
}

TEST_CASE("Synthetic market generates no events when event count is zero")
{
    auto config = make_config();
    config.event_count = 0;

    SyntheticMarket market{config};

    Event event{};

    REQUIRE_FALSE(market.next(event));
}

TEST_CASE("Synthetic market generation is deterministic for same seed")
{
    auto config = make_config();
    config.event_count = 100;

    const auto events1 = generate_events(config);
    const auto events2 = generate_events(config);

    REQUIRE(events1 == events2);
}

TEST_CASE("Synthetic market generation changes with seed")
{
    auto config1 = make_config();
    config1.event_count = 100;

    auto config2 = config1;
    config2.seed = 54321;

    const auto events1 = generate_events(config1);
    const auto events2 = generate_events(config2);

    REQUIRE(events1 != events2);
}

TEST_CASE("Synthetic market event sequences are strictly increasing")
{
    auto config = make_config();
    config.event_count = 100;

    SyntheticMarket market{config};

    Event event{};
    std::uint64_t previous_sequence{};

    bool first = true;

    while (market.next(event))
    {
        if (!first)
            REQUIRE(event.sequence > previous_sequence);

        previous_sequence = event.sequence;
        first = false;
    }
}

TEST_CASE("Synthetic market generates order IDs in increasing order")
{
    auto config = make_config();
    config.event_count = 100;
    config.cancel_probability = 0.0;

    SyntheticMarket market{config};

    Event event{};
    OrderID expected_id{};

    while (market.next(event))
    {
        REQUIRE(std::holds_alternative<LimitOrderEvent>(
            event.payload));

        const auto &order =
            std::get<LimitOrderEvent>(event.payload);

        REQUIRE(order.id == expected_id);

        ++expected_id;
    }
}

TEST_CASE("Synthetic market generates prices within configured range")
{
    auto config = make_config();
    config.event_count = 1000;
    config.cancel_probability = 0.0;

    SyntheticMarket market{config};

    Event event{};

    while (market.next(event))
    {
        REQUIRE(std::holds_alternative<LimitOrderEvent>(
            event.payload));

        const auto &order =
            std::get<LimitOrderEvent>(event.payload);

        REQUIRE(
            order.price >=
            config.reference_price + config.min_price_offset);

        REQUIRE(
            order.price <=
            config.reference_price + config.max_price_offset);
    }
}

TEST_CASE("Synthetic market generates quantities within configured range")
{
    auto config = make_config();
    config.event_count = 1000;
    config.cancel_probability = 0.0;

    SyntheticMarket market{config};

    Event event{};

    while (market.next(event))
    {
        REQUIRE(std::holds_alternative<LimitOrderEvent>(
            event.payload));

        const auto &order =
            std::get<LimitOrderEvent>(event.payload);

        REQUIRE(order.quantity >= config.min_quantity);
        REQUIRE(order.quantity <= config.max_quantity);
    }
}

TEST_CASE("Synthetic market can generate both buy and sell orders")
{
    auto config = make_config();
    config.event_count = 1000;
    config.cancel_probability = 0.0;
    config.buy_probability = 0.5;

    SyntheticMarket market{config};

    Event event{};

    bool generated_buy = false;
    bool generated_sell = false;

    while (market.next(event))
    {
        REQUIRE(std::holds_alternative<LimitOrderEvent>(
            event.payload));

        const auto &order =
            std::get<LimitOrderEvent>(event.payload);

        if (order.side == Side::Buy)
            generated_buy = true;
        else
            generated_sell = true;
    }

    REQUIRE(generated_buy);
    REQUIRE(generated_sell);
}

TEST_CASE("Synthetic market does not generate cancellation without active orders")
{
    auto config = make_config();
    config.event_count = 100;
    config.cancel_probability = 1.0;

    SyntheticMarket market{config};

    Event event{};

    while (market.next(event))
    {
        REQUIRE(std::holds_alternative<LimitOrderEvent>(
            event.payload));
    }
}

TEST_CASE("Synthetic market generates cancellation for resting order")
{
    auto config = make_config();
    config.event_count = 2;
    config.cancel_probability = 1.0;

    SyntheticMarket market{config};

    Event event{};

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<LimitOrderEvent>(
        event.payload));

    const auto &order =
        std::get<LimitOrderEvent>(event.payload);

    const OrderID order_id = order.id;

    market.on_order_update(
        order_id,
        OrderStatus::RESTING);

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<CancelOrderEvent>(
        event.payload));

    const auto &cancel =
        std::get<CancelOrderEvent>(event.payload);

    REQUIRE(cancel.id == order_id);
}

TEST_CASE("Synthetic market does not cancel removed order")
{
    auto config = make_config();
    config.event_count = 2;
    config.cancel_probability = 1.0;

    SyntheticMarket market{config};

    Event event{};

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<LimitOrderEvent>(
        event.payload));

    const auto &order =
        std::get<LimitOrderEvent>(event.payload);

    const OrderID order_id = order.id;

    market.on_order_update(
        order_id,
        OrderStatus::RESTING);

    market.on_order_update(
        order_id,
        OrderStatus::REMOVED);

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<LimitOrderEvent>(
        event.payload));
}

TEST_CASE("Synthetic market cancellation always targets active order")
{
    auto config = make_config();
    config.event_count = 100;
    config.cancel_probability = 1.0;

    SyntheticMarket market{config};

    Event event{};

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<LimitOrderEvent>(
        event.payload));

    const auto &first_order =
        std::get<LimitOrderEvent>(event.payload);

    market.on_order_update(
        first_order.id,
        OrderStatus::RESTING);

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<CancelOrderEvent>(
        event.payload));

    const auto &cancel =
        std::get<CancelOrderEvent>(event.payload);

    REQUIRE(cancel.id == first_order.id);
}

TEST_CASE("Synthetic market maintains active orders after removals")
{
    auto config = make_config();
    config.event_count = 6;
    config.cancel_probability = 1.0;

    SyntheticMarket market{config};

    Event event{};

    // Create first order.
    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<LimitOrderEvent>(
        event.payload));

    const auto first_order =
        std::get<LimitOrderEvent>(event.payload);

    market.on_order_update(
        first_order.id,
        OrderStatus::RESTING);

    // Create second order.
    //
    // The event source cannot generate a cancellation here because
    // cancel_probability is 1.0, so temporarily create the order
    // through a fresh market configuration is not possible.
    //
    // Instead, the following sequence repeatedly cancels and then
    // creates a new order through the normal feedback mechanism.

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<CancelOrderEvent>(
        event.payload));

    const auto first_cancel =
        std::get<CancelOrderEvent>(event.payload);

    REQUIRE(first_cancel.id == first_order.id);

    market.on_order_update(
        first_order.id,
        OrderStatus::REMOVED);

    // With no active orders, the source must generate a new order
    // even though cancellation probability is 1.0.
    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<LimitOrderEvent>(
        event.payload));

    const auto second_order =
        std::get<LimitOrderEvent>(event.payload);

    REQUIRE(second_order.id != first_order.id);

    market.on_order_update(
        second_order.id,
        OrderStatus::RESTING);

    REQUIRE(market.next(event));
    REQUIRE(std::holds_alternative<CancelOrderEvent>(
        event.payload));

    const auto second_cancel =
        std::get<CancelOrderEvent>(event.payload);

    REQUIRE(second_cancel.id == second_order.id);
}