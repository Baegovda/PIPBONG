#pragma once

#include "model/FeatureRunMode.h"

#include <vector>

enum class TriggerSessionPhase {
    None,
    Monitoring,
    RunningAction,
    Cooldown
};
struct SessionRunPolicyInput {
    FeatureRunMode runningMode = FeatureRunMode::RepeatCount;
    TriggerSessionPhase triggerPhase = TriggerSessionPhase::None;
    bool repeatSession = false;
    bool holdRunActive = false;
    int repeatRemaining = 0;
    bool engineRunning = false;
    int lockMouseDuringFirstLoopCount = 0;
    bool earlyLoopMouseLockReleased = false;
    int runLoopNumber = 1;
    int sessionIteration = 0;
};

enum class WorkflowRefreshDecision {
    SkipInactive,
    Immediate,
    Defer
};

namespace SessionRunPolicy {

bool isSessionActive(const SessionRunPolicyInput& session);
bool isInActiveWorkflowRun(const SessionRunPolicyInput& session);
bool isEarlyLoopMouseLockWindow(const SessionRunPolicyInput& session);

bool hasAnyActiveWorkflowEngine(const std::vector<SessionRunPolicyInput>& sessions);
bool hasAnyRunningSession(const std::vector<SessionRunPolicyInput>& sessions);

WorkflowRefreshDecision workflowRefreshOnProjectEdit(const SessionRunPolicyInput& session);

std::vector<int> activeWorkflowSessionIndices(const std::vector<SessionRunPolicyInput>& sessions);

} // namespace SessionRunPolicy
