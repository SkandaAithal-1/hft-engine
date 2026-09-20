#include <hft/event/event_processor.hpp>

EventProcessor::EventProcessor(MatchingEngine &engine) : _engine(engine) {}

MatchingResult EventProcessor::process_event(const LimitOrderEvent &event)
{
    return _engine.process_order(event.id, event.side, event.price, event.quantity);
}

CancelResult EventProcessor::process_event(const CancelOrderEvent &event)
{
    return _engine.cancel_order(event.id);
}

ProcessResult EventProcessor::process(const Event &event)
{
    return std::visit(
        [this](const auto &input) -> ProcessResult
        {
            return process_event(input);
        },
        event.payload
    );
}