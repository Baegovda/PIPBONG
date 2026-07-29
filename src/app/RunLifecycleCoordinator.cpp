#include "app/RunLifecycleCoordinator.h"

#include "app/FeatureRunSession.h"
#include "app/SessionRunPolicy.h"

SessionRunPolicyInput RunLifecycleCoordinator::policyInputFrom(const FeatureRunSession& session) {
    SessionRunPolicyInput input;
    input.runningMode = session.runningMode;
    input.triggerPhase = session.triggerPhase;
    input.repeatSession = session.repeatSession;
    input.holdRunActive = session.holdRunActive;
    input.repeatRemaining = session.repeatRemaining;
    input.engineRunning = (session.engine && session.engine->isRunning())
                          || session.holdKeyTapLaneActive;
    input.lockMouseDuringFirstLoopCount = session.lockMouseDuringFirstLoopCount;
    input.earlyLoopMouseLockReleased = session.earlyLoopMouseLockReleased;
    input.sessionIteration = session.sessionIteration;
    input.runLoopNumber =
        session.sessionContext ? session.sessionContext->runLoopNumber() : 0;
    return input;
}

std::vector<SessionRunPolicyInput> RunLifecycleCoordinator::policyInputsFromSessions(
    const std::map<std::string, FeatureRunSession>& sessions,
    const std::string* excludeFeatureId) {
    std::vector<SessionRunPolicyInput> inputs;
    inputs.reserve(sessions.size());
    for (const auto& entry : sessions) {
        if (excludeFeatureId && entry.first == *excludeFeatureId) {
            continue;
        }
        inputs.push_back(policyInputFrom(entry.second));
    }
    return inputs;
}

bool RunLifecycleCoordinator::shouldContinueSession(const FeatureRunSession& session,
                                                    bool holdBindingStillActive) {
    if (session.userStopRequested) {
        return false;
    }
    return SessionRunPolicy::shouldContinueSession(policyInputFrom(session), holdBindingStillActive);
}
