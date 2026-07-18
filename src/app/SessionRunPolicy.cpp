#include "app/SessionRunPolicy.h"

namespace SessionRunPolicy {

bool isSessionActive(const SessionRunPolicyInput& session) {
    if (session.engineRunning) {
        return true;
    }
    if (session.runningMode == FeatureRunMode::Hold && session.holdRunActive) {
        return true;
    }
    if (session.runningMode == FeatureRunMode::Trigger && session.repeatSession) {
        return true;
    }
    if (session.repeatSession
        && (session.runningMode == FeatureRunMode::RepeatInfinite
            || session.runningMode == FeatureRunMode::RepeatCount)) {
        return true;
    }
    return false;
}

bool isInActiveWorkflowRun(const SessionRunPolicyInput& session) {
    if (!isSessionActive(session)) {
        return false;
    }
    if (session.runningMode == FeatureRunMode::Trigger) {
        return session.triggerPhase == TriggerSessionPhase::RunningAction;
    }
    return true;
}

bool isEarlyLoopMouseLockWindow(const SessionRunPolicyInput& session) {
    if (session.lockMouseDuringFirstLoopCount <= 0 || session.earlyLoopMouseLockReleased) {
        return false;
    }
    if (session.runningMode == FeatureRunMode::Trigger
        && session.triggerPhase != TriggerSessionPhase::RunningAction) {
        return false;
    }
    const int loopNumber =
        session.runLoopNumber > 0 ? session.runLoopNumber : session.sessionIteration + 1;
    return loopNumber <= session.lockMouseDuringFirstLoopCount;
}

bool hasAnyActiveWorkflowEngine(const std::vector<SessionRunPolicyInput>& sessions) {
    for (const SessionRunPolicyInput& session : sessions) {
        if (session.engineRunning) {
            return true;
        }
    }
    return false;
}

bool hasAnyRunningSession(const std::vector<SessionRunPolicyInput>& sessions) {
    for (const SessionRunPolicyInput& session : sessions) {
        if (isSessionActive(session)) {
            return true;
        }
    }
    return false;
}

WorkflowRefreshDecision workflowRefreshOnProjectEdit(const SessionRunPolicyInput& session) {
    if (!isSessionActive(session)) {
        return WorkflowRefreshDecision::SkipInactive;
    }
    if (session.engineRunning) {
        return WorkflowRefreshDecision::Defer;
    }
    return WorkflowRefreshDecision::Immediate;
}

std::vector<int> activeWorkflowSessionIndices(const std::vector<SessionRunPolicyInput>& sessions) {
    std::vector<int> indices;
    indices.reserve(sessions.size());
    for (int i = 0; i < static_cast<int>(sessions.size()); ++i) {
        if (isInActiveWorkflowRun(sessions[static_cast<size_t>(i)])) {
            indices.push_back(i);
        }
    }
    return indices;
}

} // namespace SessionRunPolicy
