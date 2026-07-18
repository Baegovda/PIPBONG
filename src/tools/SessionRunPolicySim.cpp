#include "app/SessionRunPolicy.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

void expect(bool condition, const char* label) {
    if (condition) {
        return;
    }
    ++gFailures;
    std::cerr << "FAIL: " << label << '\n';
}

SessionRunPolicyInput triggerWatch(bool engineRunning = true) {
    SessionRunPolicyInput session;
    session.runningMode = FeatureRunMode::Trigger;
    session.triggerPhase = TriggerSessionPhase::Monitoring;
    session.repeatSession = true;
    session.engineRunning = engineRunning;
    return session;
}

SessionRunPolicyInput triggerCooldown() {
    SessionRunPolicyInput session;
    session.runningMode = FeatureRunMode::Trigger;
    session.triggerPhase = TriggerSessionPhase::Cooldown;
    session.repeatSession = true;
    session.engineRunning = false;
    return session;
}

SessionRunPolicyInput triggerAction(bool engineRunning = true) {
    SessionRunPolicyInput session;
    session.runningMode = FeatureRunMode::Trigger;
    session.triggerPhase = TriggerSessionPhase::RunningAction;
    session.repeatSession = true;
    session.engineRunning = engineRunning;
    session.runLoopNumber = 1;
    return session;
}

SessionRunPolicyInput repeatInfinite(bool engineRunning = true) {
    SessionRunPolicyInput session;
    session.runningMode = FeatureRunMode::RepeatInfinite;
    session.repeatSession = true;
    session.engineRunning = engineRunning;
    return session;
}

SessionRunPolicyInput holdActive(bool engineRunning = false) {
    SessionRunPolicyInput session;
    session.runningMode = FeatureRunMode::Hold;
    session.holdRunActive = true;
    session.repeatSession = true;
    session.engineRunning = engineRunning;
    return session;
}

void runTriggerWatchScenarios() {
    const SessionRunPolicyInput watch = triggerWatch(true);
    expect(SessionRunPolicy::isSessionActive(watch), "trigger watch is active session");
    expect(!SessionRunPolicy::isInActiveWorkflowRun(watch), "trigger watch is not active workflow");
    expect(SessionRunPolicy::workflowRefreshOnProjectEdit(watch) == WorkflowRefreshDecision::Defer,
           "trigger watch defers workflow refresh while engine runs");
    expect(!SessionRunPolicy::isEarlyLoopMouseLockWindow(watch), "trigger watch skips early-loop lock");

    SessionRunPolicyInput watchIdle = triggerWatch(false);
    expect(SessionRunPolicy::isSessionActive(watchIdle), "trigger watch idle gap stays session");
    expect(!SessionRunPolicy::isInActiveWorkflowRun(watchIdle), "trigger watch idle not workflow");
    expect(SessionRunPolicy::workflowRefreshOnProjectEdit(watchIdle)
               == WorkflowRefreshDecision::Immediate,
           "trigger watch idle applies workflow immediately");
}

void runTriggerActionScenarios() {
    const SessionRunPolicyInput action = triggerAction(true);
    expect(SessionRunPolicy::isInActiveWorkflowRun(action), "trigger action is active workflow");
    expect(SessionRunPolicy::workflowRefreshOnProjectEdit(action) == WorkflowRefreshDecision::Defer,
           "trigger action defers workflow edit");
    expect(SessionRunPolicy::hasAnyActiveWorkflowEngine({action}), "trigger action blocks update");

    SessionRunPolicyInput earlyLock = triggerAction(false);
    earlyLock.lockMouseDuringFirstLoopCount = 3;
    earlyLock.runLoopNumber = 2;
    expect(SessionRunPolicy::isEarlyLoopMouseLockWindow(earlyLock),
           "trigger action loop 2 within early-lock window");

    earlyLock.runLoopNumber = 4;
    expect(!SessionRunPolicy::isEarlyLoopMouseLockWindow(earlyLock),
           "trigger action loop 4 past early-lock window");

    earlyLock.runLoopNumber = 1;
    earlyLock.earlyLoopMouseLockReleased = true;
    expect(!SessionRunPolicy::isEarlyLoopMouseLockWindow(earlyLock),
           "early-lock released disables window");
}

void runMultiFeatureScenarios() {
    const std::vector<SessionRunPolicyInput> watchPlusIdle = {triggerWatch(false), repeatInfinite(false)};
    expect(SessionRunPolicy::hasAnyRunningSession(watchPlusIdle), "watch + idle repeat still has session");
    expect(!SessionRunPolicy::hasAnyActiveWorkflowEngine(watchPlusIdle),
           "watch without engine does not block update");
    const auto active = SessionRunPolicy::activeWorkflowSessionIndices(watchPlusIdle);
    expect(active.size() == 1 && active[0] == 1,
           "between-loop infinite repeat is active workflow; trigger watch is not");

    const std::vector<SessionRunPolicyInput> watchPlusAction = {triggerWatch(true), triggerAction(true)};
    expect(SessionRunPolicy::hasAnyActiveWorkflowEngine(watchPlusAction), "any engine blocks update");
    const auto activeIndices = SessionRunPolicy::activeWorkflowSessionIndices(watchPlusAction);
    expect(activeIndices.size() == 1 && activeIndices[0] == 1,
           "only running-action session is active workflow");
}

void runLegacyModeScenarios() {
    const SessionRunPolicyInput infinite = repeatInfinite(true);
    expect(SessionRunPolicy::isInActiveWorkflowRun(infinite), "infinite repeat is active workflow");
    expect(SessionRunPolicy::workflowRefreshOnProjectEdit(infinite) == WorkflowRefreshDecision::Defer,
           "infinite repeat defers while engine runs");

    SessionRunPolicyInput hold = holdActive(true);
    expect(SessionRunPolicy::isInActiveWorkflowRun(hold), "hold with engine is active workflow");

    SessionRunPolicyInput repeatCount;
    repeatCount.runningMode = FeatureRunMode::RepeatCount;
    repeatCount.repeatSession = true;
    repeatCount.repeatRemaining = 2;
    repeatCount.engineRunning = false;
    expect(SessionRunPolicy::isSessionActive(repeatCount), "repeat count between loops stays session");
    expect(SessionRunPolicy::isInActiveWorkflowRun(repeatCount), "repeat count between loops blocks edit");
}

void runCooldownScenarios() {
    const SessionRunPolicyInput cooldown = triggerCooldown();
    expect(SessionRunPolicy::isSessionActive(cooldown), "trigger cooldown is session");
    expect(!SessionRunPolicy::isInActiveWorkflowRun(cooldown), "cooldown is not active workflow");
    expect(!SessionRunPolicy::hasAnyActiveWorkflowEngine({cooldown}), "cooldown does not block update");
    expect(SessionRunPolicy::workflowRefreshOnProjectEdit(cooldown) == WorkflowRefreshDecision::Immediate,
           "cooldown applies workflow edits immediately");
}

void runInactiveScenarios() {
    SessionRunPolicyInput inactive;
    expect(!SessionRunPolicy::isSessionActive(inactive), "default session inactive");
    expect(SessionRunPolicy::workflowRefreshOnProjectEdit(inactive) == WorkflowRefreshDecision::SkipInactive,
           "inactive session skips workflow refresh");
}

void runCenterPinPolicy() {
    const std::vector<SessionRunPolicyInput> watchOnly = {triggerWatch(false)};
    expect(SessionRunPolicy::hasAnyRunningSession(watchOnly),
           "center pin still suppressed during trigger watch");
    expect(!SessionRunPolicy::hasAnyActiveWorkflowEngine(watchOnly),
           "update allowed during trigger watch");
}

void runHoldBetweenLoopsScenarios() {
    SessionRunPolicyInput holdIdle = holdActive(false);
    expect(SessionRunPolicy::isSessionActive(holdIdle), "hold between engine bursts stays session");
    expect(SessionRunPolicy::isInActiveWorkflowRun(holdIdle),
           "hold between engine bursts is active workflow");
    expect(!SessionRunPolicy::hasAnyActiveWorkflowEngine({holdIdle}), "hold idle does not block update");
}

void runStaleSessionEntryScenarios() {
    SessionRunPolicyInput stale;
    stale.repeatSession = false;
    stale.engineRunning = false;
    expect(!SessionRunPolicy::isSessionActive(stale), "stale map entry is not active session");
    expect(!SessionRunPolicy::hasAnyRunningSession({stale}), "stale entry does not block center pin");
}

} // namespace

int main() {
    runInactiveScenarios();
    runTriggerWatchScenarios();
    runTriggerActionScenarios();
    runCooldownScenarios();
    runLegacyModeScenarios();
    runMultiFeatureScenarios();
    runCenterPinPolicy();
    runHoldBetweenLoopsScenarios();
    runStaleSessionEntryScenarios();

    if (gFailures == 0) {
        std::cout << "SessionRunPolicySim: all scenarios passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "SessionRunPolicySim: " << gFailures << " scenario(s) failed.\n";
    return EXIT_FAILURE;
}
