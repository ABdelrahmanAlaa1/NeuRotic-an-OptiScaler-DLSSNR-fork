#pragma once

#include <cstdint>

namespace DlssNr
{
// Value-only reporting policy. A retained model handle is not evidence that the currently
// requested route has evaluated. Inputs are copied while the backend state is locked.
struct ReadinessInput
{
    bool enabled = false;
    bool sessionOpen = false;
    bool failed = false;
    bool featureLoaded = false;
    bool evaluated = false;
    bool resetPending = true;
    uint64_t requestedGeneration = 0;
    uint64_t evaluatedGeneration = 0;
    bool preSrRequested = false;
    bool lastEvaluationWasPreSr = false;
    bool scratchPrimed = false;
    bool awaitingEvaluation = false;
};

struct Readiness
{
    bool running = false;
    bool preSrDisplayReady = false;
    bool transitionPending = false;
    bool outputQuarantined = false;
};

inline Readiness GetReadiness(const ReadinessInput& input)
{
    Readiness result;
    const bool admitted = input.enabled && input.sessionOpen && !input.failed;
    const bool evaluated = admitted && input.featureLoaded && input.evaluated && !input.resetPending &&
                           input.requestedGeneration == input.evaluatedGeneration &&
                           input.preSrRequested == input.lastEvaluationWasPreSr;
    result.preSrDisplayReady = evaluated && input.preSrRequested && input.scratchPrimed &&
                               !input.awaitingEvaluation;
    result.running = evaluated && (!input.preSrRequested || result.preSrDisplayReady);
    result.transitionPending = admitted && !result.running;
    result.outputQuarantined = admitted && input.preSrRequested && input.awaitingEvaluation;
    return result;
}
}
