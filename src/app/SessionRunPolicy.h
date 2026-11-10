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
/// First-ROI UI gate, in addition to existing stop/target/engine launch gates.
bool allowsPreparedLaunch(bool awaitingFirstTemplateRoi);

bool isSessionActive(const SessionRunPolicyInput& session);
bool isInActiveWorkflowRun(const SessionRunPolicyInput& session);
bool isEarlyLoopMouseLockWindow(const SessionRunPolicyInput& session);

bool hasAnyActiveWorkflowEngine(const std::vector<SessionRunPolicyInput>& sessions);
/// Hold / repeat / trigger-action bursts that compete with trigger-watch screen capture (not trigger monitor polls).
bool isCapturingWorkflowBurst(const SessionRunPolicyInput& session);
bool hasAnyCapturingWorkflowBurst(const std::vector<SessionRunPolicyInput>& sessions);
bool hasAnyRunningSession(const std::vector<SessionRunPolicyInput>& sessions);
/// Explicit target rebinding waits until all repeat/hold/trigger sessions are inactive.
bool allowsTargetBindingChange(const std::vector<SessionRunPolicyInput>& sessions);
/// No center movement during active workflow bursts, including other profiles. Monitor/cooldown is allowed.
bool allowsTargetCenterPin(const std::vector<SessionRunPolicyInput>& sessions);
bool shouldAdoptCaptureTarget(const SessionRunPolicyInput& session,bool lockedEmpty,bool lockedMissing,bool differs);

WorkflowRefreshDecision workflowRefreshOnProjectEdit(const SessionRunPolicyInput& session);

std::vector<int> activeWorkflowSessionIndices(const std::vector<SessionRunPolicyInput>& sessions);

/// Loop / trigger monitor continuation after a workflow iteration (Hold needs physical latch).
bool shouldContinueSession(const SessionRunPolicyInput& session, bool holdBindingStillActive);

/// Active sessions with `repeatSession` set (Hold / infinite / N-repeat loop gap UI coalesce).
int countActiveRepeatSessions(const std::vector<SessionRunPolicyInput>& sessions);
int fastRepeatUiFlushIntervalMs(int activeRepeatSessions);

/// Coalesce run UI refreshes when multiple sessions or 2+ active repeat loops compete (§8.21 R2.3).
bool shouldCoalesceRunUiUpdates(const std::vector<SessionRunPolicyInput>& sessions);

/// Debounce interval for `MainWindow::updateRunUiState` (ms) based on session load.
int runUiDebounceIntervalMs(const std::vector<SessionRunPolicyInput>& sessions,
                            std::size_t sessionMapSize);

} // namespace SessionRunPolicy
