#include <catch2/catch_test_macros.hpp>

#include <hft/simulation/simulation.hpp>
#include <hft/event/event_source.hpp>
#include <hft/event/event_processor.hpp>
#include <hft/event/types.hpp>
#include <hft/matching_engine/matching_engine.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>
#include <hft/order_gateway/order_gateway.hpp>
#include <hft/strategy/strategy.hpp>
#include <hft/strategy/types.hpp>
#include <hft/market_data/types.hpp>

#include <cstddef>
#include <variant>
#include <vector>


class TestEventSource : public EventSource
{
public:
    explicit TestEventSource(const std::vector<Event> &events)
        : _events(events)
    {
    }

    bool next(Event &event) override
    {
        if (_index >= _events.size())
            return false;

        event = _events[_index++];
        return true;
    }

private:
    const std::vector<Event> &_events;
    std::size_t _index{};
};


class TestStrategy : public Strategy
{
public:
    explicit TestStrategy(OrderGateway &gateway)
        : Strategy(gateway)
    {
    }

    void on_execution(const ExecutionReport &report) override
    {
        executions.push_back(report);
    }

    void on_market_data(const MarketEvent &event) override
    {
        market_events.push_back(event);
    }

    std::vector<ExecutionReport> executions;
    std::vector<MarketEvent> market_events;
};


class SubmittingStrategy : public Strategy
{
public:
    explicit SubmittingStrategy(OrderGateway &gateway)
        : Strategy(gateway)
    {
    }

    void on_execution(const ExecutionReport &report) override
    {
        executions.push_back(report);
    }

    void on_market_data(const MarketEvent &event) override
    {
        market_events.push_back(event);

        if (!submitted &&
            std::holds_alternative<BookUpdate>(event))
        {
            submitted = true;

            _gateway.submit_order(
                order_id,
                side,
                price,
                quantity
            );
        }
    }

    OrderID order_id{};
    Side side{};
    Price price{};
    Quantity quantity{};

    bool submitted{};

    std::vector<ExecutionReport> executions;
    std::vector<MarketEvent> market_events;
};


class OrderingStrategy : public Strategy
{
public:
    explicit OrderingStrategy(OrderGateway &gateway)
        : Strategy(gateway)
    {
    }

    void on_execution(const ExecutionReport &report) override
    {
        events.push_back(1);

        REQUIRE(report.id == 10);
        REQUIRE(report.price == 100);
        REQUIRE(report.quantity == 40);
    }

    void on_market_data(const MarketEvent &event) override
    {
        if (std::holds_alternative<BookUpdate>(event) && !submitted)
        {
            submitted = true;

            _gateway.submit_order(
                10,
                Side::Buy,
                100,
                40
            );
        }
        else if (std::holds_alternative<TradeEvent>(event))
        {
            events.push_back(2);
        }
    }

    bool submitted{};

    std::vector<int> events;
};


class CancellingStrategy : public Strategy
{
public:
    explicit CancellingStrategy(OrderGateway &gateway)
        : Strategy(gateway)
    {
    }

    void on_execution(const ExecutionReport &report) override
    {
        executions.push_back(report);
    }

    void on_market_data(const MarketEvent &event) override
    {
        market_events.push_back(event);

        if (!cancelled &&
            std::holds_alternative<BookUpdate>(event))
        {
            cancelled = true;
            _gateway.cancel_order(order_id);
        }
    }

    OrderID order_id{};

    bool cancelled{};

    std::vector<ExecutionReport> executions;
    std::vector<MarketEvent> market_events;
};


TEST_CASE("Simulation publishes BookUpdate when BBO changes")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    TestStrategy strategy{gateway};

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Buy, 100, 50}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(strategy.market_events.size() == 1);

    REQUIRE(std::holds_alternative<BookUpdate>(
        strategy.market_events[0]));

    const auto &update =
        std::get<BookUpdate>(strategy.market_events[0]);

    REQUIRE(update.best_bid == 100);
    REQUIRE(update.bid_quantity == 50);
    REQUIRE(update.best_ask == 0);
    REQUIRE(update.ask_quantity == 0);
}


TEST_CASE("Simulation publishes best bid and ask quantities")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    TestStrategy strategy{gateway};

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Buy, 100, 30}
        },
        Event{
            2,
            LimitOrderEvent{2, Side::Buy, 100, 20}
        },
        Event{
            3,
            LimitOrderEvent{3, Side::Sell, 110, 40}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(strategy.market_events.size() == 3);

    const auto &update =
        std::get<BookUpdate>(strategy.market_events[2]);

    REQUIRE(update.best_bid == 100);
    REQUIRE(update.bid_quantity == 50);
    REQUIRE(update.best_ask == 110);
    REQUIRE(update.ask_quantity == 40);
}


TEST_CASE("Simulation does not publish unchanged BBO")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    TestStrategy strategy{gateway};

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Buy, 100, 50}
        },
        Event{
            2,
            LimitOrderEvent{2, Side::Sell, 110, 50}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(strategy.market_events.size() == 2);

    const auto &first_update =
        std::get<BookUpdate>(strategy.market_events[0]);

    REQUIRE(first_update.best_bid == 100);
    REQUIRE(first_update.bid_quantity == 50);
    REQUIRE(first_update.best_ask == 0);
    REQUIRE(first_update.ask_quantity == 0);

    const auto &second_update =
        std::get<BookUpdate>(strategy.market_events[1]);

    REQUIRE(second_update.best_bid == 100);
    REQUIRE(second_update.bid_quantity == 50);
    REQUIRE(second_update.best_ask == 110);
    REQUIRE(second_update.ask_quantity == 50);
}


TEST_CASE("Simulation publishes TradeEvent for external trade")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    TestStrategy strategy{gateway};

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Sell, 100, 50}
        },
        Event{
            2,
            LimitOrderEvent{2, Side::Buy, 100, 20}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(trade_sink.size() == 1);

    REQUIRE(trade_sink[0].aggressor_id == 2);
    REQUIRE(trade_sink[0].resting_id == 1);
    REQUIRE(trade_sink[0].price == 100);
    REQUIRE(trade_sink[0].quantity == 20);

    REQUIRE(strategy.executions.empty());

    REQUIRE(strategy.market_events.size() == 3);

    REQUIRE(std::holds_alternative<BookUpdate>(
        strategy.market_events[0]));

    REQUIRE(std::holds_alternative<TradeEvent>(
        strategy.market_events[1]));

    const auto &trade_event =
        std::get<TradeEvent>(strategy.market_events[1]);

    REQUIRE(trade_event.price == 100);
    REQUIRE(trade_event.quantity == 20);
}


TEST_CASE("Strategy order is queued and processed by simulation")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    SubmittingStrategy strategy{gateway};

    strategy.order_id = 10;
    strategy.side = Side::Buy;
    strategy.price = 99;
    strategy.quantity = 40;

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Sell, 100, 50}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(strategy.submitted);
    REQUIRE(order_queue.empty());

    auto *order = book.get_order(10);

    REQUIRE(order != nullptr);
    REQUIRE(order->id == 10);
    REQUIRE(order->side == Side::Buy);
    REQUIRE(order->price == 99);
    REQUIRE(order->quantity == 40);
    REQUIRE(order->remaining == 40);
}


TEST_CASE("Strategy aggressive order receives execution report")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    SubmittingStrategy strategy{gateway};

    strategy.order_id = 10;
    strategy.side = Side::Buy;
    strategy.price = 100;
    strategy.quantity = 40;

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Sell, 100, 50}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(trade_sink.size() == 1);

    REQUIRE(trade_sink[0].aggressor_id == 10);
    REQUIRE(trade_sink[0].resting_id == 1);
    REQUIRE(trade_sink[0].price == 100);
    REQUIRE(trade_sink[0].quantity == 40);

    REQUIRE(strategy.executions.size() == 1);

    REQUIRE(strategy.executions[0].id == 10);
    REQUIRE(strategy.executions[0].price == 100);
    REQUIRE(strategy.executions[0].quantity == 40);

    REQUIRE(book.get_order(10) == nullptr);

    auto *resting_order = book.get_order(1);
    REQUIRE(resting_order != nullptr);
    REQUIRE(resting_order->remaining == 10);
}


TEST_CASE("Strategy resting order receives execution report")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    SubmittingStrategy strategy{gateway};

    strategy.order_id = 10;
    strategy.side = Side::Buy;
    strategy.price = 99;
    strategy.quantity = 100;

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Sell, 100, 50}
        },
        Event{
            2,
            LimitOrderEvent{2, Side::Sell, 99, 40}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(trade_sink.size() == 1);

    REQUIRE(trade_sink[0].aggressor_id == 2);
    REQUIRE(trade_sink[0].resting_id == 10);
    REQUIRE(trade_sink[0].price == 99);
    REQUIRE(trade_sink[0].quantity == 40);

    REQUIRE(strategy.executions.size() == 1);

    REQUIRE(strategy.executions[0].id == 10);
    REQUIRE(strategy.executions[0].price == 99);
    REQUIRE(strategy.executions[0].quantity == 40);

    auto *order = book.get_order(10);

    REQUIRE(order != nullptr);
    REQUIRE(order->remaining == 60);
}


TEST_CASE("Strategy order receives multiple execution reports")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    SubmittingStrategy strategy{gateway};

    strategy.order_id = 10;
    strategy.side = Side::Buy;
    strategy.price = 101;
    strategy.quantity = 100;

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Sell, 100, 40}
        },
        Event{
            2,
            LimitOrderEvent{2, Side::Sell, 101, 60}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(trade_sink.size() == 2);

    REQUIRE(strategy.executions.size() == 2);

    REQUIRE(strategy.executions[0].id == 10);
    REQUIRE(strategy.executions[0].price == 100);
    REQUIRE(strategy.executions[0].quantity == 40);

    REQUIRE(strategy.executions[1].id == 10);
    REQUIRE(strategy.executions[1].price == 101);
    REQUIRE(strategy.executions[1].quantity == 60);

    REQUIRE(book.get_order(10) == nullptr);
}


TEST_CASE("Execution report is published before public TradeEvent")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    OrderingStrategy strategy{gateway};

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Sell, 100, 50}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(strategy.submitted);
    REQUIRE(strategy.events.size() == 2);
    REQUIRE(strategy.events[0] == 1);
    REQUIRE(strategy.events[1] == 2);
}


TEST_CASE("Strategy cancellation removes resting order")
{
    OrderBook book;
    TradeSink trade_sink;

    MatchingEngine engine{book, trade_sink};
    EventProcessor event_processor{engine};

    OrderQueue order_queue;
    OrderGateway gateway{order_queue};
    CancellingStrategy strategy{gateway};

    strategy.order_id = 10;

    std::vector<Event> events{
        Event{
            1,
            LimitOrderEvent{1, Side::Buy, 99, 40}
        }
    };

    TestEventSource event_source{events};

    Simulation simulation{
        book,
        event_source,
        event_processor,
        engine,
        strategy,
        trade_sink,
        order_queue
    };

    simulation.run();

    REQUIRE(strategy.cancelled);
    REQUIRE(order_queue.empty());
    REQUIRE(book.get_order(10) == nullptr);
    REQUIRE(trade_sink.empty());
}