#include <hft/simulation/simulation.hpp>

Simulation::Simulation(
    OrderBook &order_book,
    EventSource &event_source,
    EventProcessor &event_processor,
    MatchingEngine &matching_engine,
    Strategy &strategy,
    TradeSink &trade_sink,
    OrderQueue &order_queue) : _order_book(order_book),
                               _event_source(event_source),
                               _event_processor(event_processor),
                               _matching_engine(matching_engine),
                               _strategy(strategy),
                               _trade_sink(trade_sink),
                               _order_queue(order_queue)
{
}

void Simulation::run()
{
    Event event{};
    while (_event_source.next(event))
    {
        const ProcessResult process_result = _event_processor.process(event);

        if (process_result.index() == 0)
        {
            const MatchingResult matching_result = std::get<0>(process_result);

            if (matching_result == MatchingResult::RESTING ||
                matching_result == MatchingResult::PARTIALLY_FILLED)
            {
                const auto order = std::get<0>(event.payload);
                _source_orders[order.id] = order.quantity;
                _event_source.on_order_update(order.id, OrderStatus::RESTING);
            }
        }
        else
        {
            const CancelResult cancel_result = std::get<1>(process_result);

            const OrderID id = std::get<1>(event.payload).id;
            if (cancel_result == CancelResult::SUCCESS)
                _source_orders.erase(id);

            // Unconditional because the order should no longer exist in source records
            _event_source.on_order_update(id, OrderStatus::REMOVED);
        }

        // This will decide which is a trade event and execution report and also check with book update and publish
        // and also change processed_event_count accordingly
        Simulation::publish_market_data();

        while (_order_queue.size())
        {
            // This will drain the order queue
            Simulation::process_order_queue();

            Simulation::publish_market_data();
        }
    }
}

void Simulation::publish_market_data()
{
    while (processed_trade_count < _trade_sink.size())
    {
        const Trade &trade = _trade_sink[processed_trade_count++];

        if (_strategy_orders.find(trade.aggressor_id) != _strategy_orders.end())
        {
            _strategy.on_execution(ExecutionReport{trade.aggressor_id, trade.price, trade.quantity});
            _strategy_orders[trade.aggressor_id] -= trade.quantity;
            if (_strategy_orders[trade.aggressor_id] == 0)
                _strategy_orders.erase(trade.aggressor_id);
        }
        else if (_source_orders.find(trade.aggressor_id) != _source_orders.end())
        {
            _source_orders[trade.aggressor_id] -= trade.quantity;
            if (_source_orders[trade.aggressor_id] == 0)
            {
                _source_orders.erase(trade.aggressor_id);
                _event_source.on_order_update(trade.aggressor_id, OrderStatus::REMOVED);
            }
        }

        if (_strategy_orders.find(trade.resting_id) != _strategy_orders.end())
        {
            _strategy.on_execution(ExecutionReport{trade.resting_id, trade.price, trade.quantity});
            _strategy_orders[trade.resting_id] -= trade.quantity;
            if (_strategy_orders[trade.resting_id] == 0)
                _strategy_orders.erase(trade.resting_id);
        }
        else if (_source_orders.find(trade.resting_id) != _source_orders.end())
        {
            _source_orders[trade.resting_id] -= trade.quantity;
            if (_source_orders[trade.resting_id] == 0)
            {
                _source_orders.erase(trade.resting_id);
                _event_source.on_order_update(trade.resting_id, OrderStatus::REMOVED);
            }
        }

        _strategy.on_market_data(TradeEvent{trade.price, trade.quantity});
    }

    const std::optional<Order> best_bid = _order_book.best_bid();
    const std::optional<Order> best_ask = _order_book.best_ask();
    BookUpdate next_update{};
    if (best_bid)
    {
        next_update.best_bid = best_bid->price;
        next_update.bid_quantity = _order_book.quantity_at(Side::Buy, best_bid->price);
    }
    if (best_ask)
    {
        next_update.best_ask = best_ask->price;
        next_update.ask_quantity = _order_book.quantity_at(Side::Sell, best_ask->price);
    }

    if (next_update != last_published_update)
    {
        _strategy.on_market_data(next_update);
        last_published_update = next_update;
    }
}

void Simulation::process_order_queue()
{
    const auto order_request = _order_queue.front();
    _order_queue.pop();

    SubmitOrderRequest submit;
    CancelOrderRequest cancel;
    switch (order_request.index())
    {
    case 0:
        submit = std::get<SubmitOrderRequest>(order_request);
        const MatchingResult matching_result = _matching_engine.process_order(submit.id, submit.side, submit.price, submit.quantity);

        if (matching_result == MatchingResult::RESTING ||
            matching_result == MatchingResult::PARTIALLY_FILLED ||
            matching_result == MatchingResult::FULLY_FILLED)
        {
            _strategy_orders[submit.id] = submit.quantity;
        }
        break;
    case 1:
        cancel = std::get<CancelOrderRequest>(order_request);
        const CancelResult cancel_result = _matching_engine.cancel_order(cancel.id);
        if (cancel_result == CancelResult::SUCCESS)
        {
            _strategy_orders.erase(cancel.id);
        }
        break;
    }
}