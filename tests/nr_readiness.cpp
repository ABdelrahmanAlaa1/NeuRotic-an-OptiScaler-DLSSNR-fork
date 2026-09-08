#include "../OptiScaler/dlssnr/NrReadiness.h"
#include <cassert>
#include <cstdio>

int main()
{
    using namespace DlssNr;
    ReadinessInput state;
    assert(!GetReadiness(state).running && !GetReadiness(state).transitionPending);
    state.enabled = state.sessionOpen = true;
    assert(GetReadiness(state).transitionPending && !GetReadiness(state).running);
    state.featureLoaded = true;
    assert(!GetReadiness(state).running); // creation alone is not evaluation
    state.evaluated = true;
    state.resetPending = false;
    assert(GetReadiness(state).running);
    state.enabled = false;
    assert(!GetReadiness(state).running && !GetReadiness(state).transitionPending);
    state.enabled = true;
    ++state.requestedGeneration;
    assert(!GetReadiness(state).running); // retained history from before the toggle
    state.evaluatedGeneration = state.requestedGeneration;
    assert(GetReadiness(state).running);
    state.preSrRequested = true;
    assert(!GetReadiness(state).running); // still reflects the previous Post-SR route
    state.lastEvaluationWasPreSr = true;
    state.awaitingEvaluation = true;
    assert(!GetReadiness(state).preSrDisplayReady && GetReadiness(state).outputQuarantined);
    state.scratchPrimed = true;
    state.awaitingEvaluation = false;
    assert(GetReadiness(state).running && GetReadiness(state).preSrDisplayReady);
    state.failed = true;
    assert(!GetReadiness(state).running && !GetReadiness(state).preSrDisplayReady &&
           !GetReadiness(state).transitionPending);
    state.failed = false;
    state.sessionOpen = false;
    assert(!GetReadiness(state).running);
    state.sessionOpen = true;
    state.resetPending = true;
    assert(!GetReadiness(state).running);
    for (unsigned i = 0; i < 10000; ++i)
    {
        state.enabled = false;
        assert(!GetReadiness(state).running);
        state.enabled = true;
        ++state.requestedGeneration;
        assert(!GetReadiness(state).running);
        state.evaluatedGeneration = state.requestedGeneration;
        state.resetPending = false;
        assert(GetReadiness(state).running);
        state.resetPending = true;
    }
    std::puts("PASS: startup, retained handles, resume generations, route transitions, failure, shutdown, 10000 toggles");
}
