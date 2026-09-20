#pragma once

#include <hft/event/types.hpp>
#include <hft/matching_engine/matching_engine.hpp>

class EventProcessor
{
public:
    explicit EventProcessor(MatchingEngine &engine);

    ProcessResult process(const Event &event);

private:
    MatchingEngine &_engine;
    MatchingResult process_event(const LimitOrderEvent &event);
    CancelResult process_event(const CancelOrderEvent &event);
};
