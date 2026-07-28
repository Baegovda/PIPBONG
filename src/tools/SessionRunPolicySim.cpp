#include "app/SessionRunPolicy.h"
#include "app/SessionRunPolicyInvariants.h"
#include "app/ForegroundRunGate.h"
#include "model/FeatureRunMode.h"
#include "model/FeatureCaptureTargetScope.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int gScenarioFailures = 0;
int gScenarioPasses = 0;
std::vector<std::string> gReportLines;

void logLine(const std::string& line) {
    gReportLines.push_back(line);
    std::cout << line << '\n';
}

void expectScenario(bool condition, const char* labelEn, const char* labelKo) {
    if (condition) {
        ++gScenarioPasses;
        return;
    }
    ++gScenarioFailures;
    std::cerr << "FAIL [" << labelEn << "]\n  " << labelKo << '\n';
    gReportLines.push_back(std::string("FAIL: ") + labelEn);
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

void runManualScenarios() {
    logLine("== Manual scenarios (v0.8.219 policy) ==");

    const SessionRunPolicyInput watch = triggerWatch(true);
    expectScenario(SessionRunPolicy::isSessionActive(watch),
                   "trigger_watch_active_session",
                   u8"트리거 감시는 활성 세션");
    expectScenario(!SessionRunPolicy::isInActiveWorkflowRun(watch),
                   "trigger_watch_not_workflow",
                   u8"트리거 감시는 워크플로 실행 중 아님");
    expectScenario(SessionRunPolicy::workflowRefreshOnProjectEdit(watch)
                       == WorkflowRefreshDecision::Defer,
                   "trigger_watch_defer_refresh",
                   u8"감시 중 엔진 돌 때 워크플로 편집 지연");
    expectScenario(!SessionRunPolicy::isEarlyLoopMouseLockWindow(watch),
                   "trigger_watch_no_early_lock",
                   u8"트리거 감시 중 초기 루프 마우스 잠금 없음");

    SessionRunPolicyInput watchIdle = triggerWatch(false);
    expectScenario(SessionRunPolicy::workflowRefreshOnProjectEdit(watchIdle)
                       == WorkflowRefreshDecision::Immediate,
                   "trigger_watch_idle_immediate_refresh",
                   u8"감시 중 엔진 멈춤 시 워크플로 편집 즉시 반영");

    const SessionRunPolicyInput action = triggerAction(true);
    expectScenario(SessionRunPolicy::isInActiveWorkflowRun(action),
                   "trigger_action_workflow",
                   u8"트리거 동작 단계는 워크플로 실행 중");
    expectScenario(SessionRunPolicy::hasAnyActiveWorkflowEngine({action}),
                   "trigger_action_blocks_update",
                   u8"트리거 동작 중 업데이트 차단");

    SessionRunPolicyInput earlyLock = triggerAction(false);
    earlyLock.lockMouseDuringFirstLoopCount = 3;
    earlyLock.runLoopNumber = 2;
    expectScenario(SessionRunPolicy::isEarlyLoopMouseLockWindow(earlyLock),
                   "early_lock_loop2",
                   u8"초기 루프 잠금 2회차 적용");
    earlyLock.runLoopNumber = 4;
    expectScenario(!SessionRunPolicy::isEarlyLoopMouseLockWindow(earlyLock),
                   "early_lock_past_window",
                   u8"초기 루프 잠금 N회 초과 시 해제");

    const std::vector<SessionRunPolicyInput> watchPlusIdle = {triggerWatch(false),
                                                              repeatInfinite(false)};
    expectScenario(SessionRunPolicy::hasAnyRunningSession(watchPlusIdle),
                   "multi_watch_plus_repeat_session",
                   u8"감시+반복 idle 모두 활성 세션");
    expectScenario(!SessionRunPolicy::hasAnyActiveWorkflowEngine(watchPlusIdle),
                   "multi_watch_no_engine_update_ok",
                   u8"감시만 있을 때 업데이트 허용");
    const auto active = SessionRunPolicy::activeWorkflowSessionIndices(watchPlusIdle);
    expectScenario(active.size() == 1 && active[0] == 1,
                   "multi_only_repeat_is_workflow",
                   u8"다기능: 반복 기능만 워크플로 활성");

    const SessionRunPolicyInput cooldown = triggerCooldown();
    expectScenario(!SessionRunPolicy::isInActiveWorkflowRun(cooldown),
                   "cooldown_not_workflow",
                   u8"트리거 쿨다운은 워크플로 실행 중 아님");
    expectScenario(SessionRunPolicy::workflowRefreshOnProjectEdit(cooldown)
                       == WorkflowRefreshDecision::Immediate,
                   "cooldown_immediate_refresh",
                   u8"쿨다운 중 워크플로 편집 즉시 반영");

    SessionRunPolicyInput repeatCount;
    repeatCount.runningMode = FeatureRunMode::RepeatCount;
    repeatCount.repeatSession = true;
    repeatCount.repeatRemaining = 2;
    repeatCount.engineRunning = false;
    expectScenario(SessionRunPolicy::isInActiveWorkflowRun(repeatCount),
                   "repeat_count_between_loops_blocks_edit",
                   u8"N회 반복 루프 사이에도 편집 차단");

    SessionRunPolicyInput holdIdle = holdActive(false);
    expectScenario(SessionRunPolicy::isInActiveWorkflowRun(holdIdle),
                   "hold_idle_workflow",
                   u8"홀드 엔진 idle 구간도 워크플로 활성");

    SessionRunPolicyInput stale;
    stale.repeatSession = false;
    expectScenario(!SessionRunPolicy::isSessionActive(stale),
                   "stale_session_inactive",
                   u8"repeatSession 꺼진 맵 항목은 비활성");

    SessionRunPolicyInput holdReleased;
    holdReleased.runningMode = FeatureRunMode::Hold;
    holdReleased.holdRunActive = false;
    holdReleased.repeatSession = false;
    holdReleased.engineRunning = false;
    expectScenario(!SessionRunPolicy::isSessionActive(holdReleased),
                   "hold_released_inactive",
                   u8"홀드 해제 후 비활성 세션");

    SessionRunPolicyInput triggerDisarmed;
    triggerDisarmed.runningMode = FeatureRunMode::Trigger;
    triggerDisarmed.repeatSession = false;
    triggerDisarmed.triggerPhase = TriggerSessionPhase::None;
    expectScenario(!SessionRunPolicy::isSessionActive(triggerDisarmed),
                   "trigger_disarmed_inactive",
                   u8"트리거 무장 해제 시 비활성");
}

void runHoldBurstScenarios() {
    logLine("== Hold / burst scenarios (v0.8.359) ==");

    SessionRunPolicyInput holdEngine = holdActive(true);
    expectScenario(SessionRunPolicy::isCapturingWorkflowBurst(holdEngine),
                   "hold_engine_capturing_burst",
                   u8"홀드 엔진 실행 중은 캡처 버스트");
    expectScenario(SessionRunPolicy::isInActiveWorkflowRun(holdEngine),
                   "hold_engine_active_workflow",
                   u8"홀드 엔진 실행 중 워크플로 활성");

    SessionRunPolicyInput holdIdle = holdActive(false);
    expectScenario(!SessionRunPolicy::isCapturingWorkflowBurst(holdIdle),
                   "hold_idle_not_capturing_burst",
                   u8"홀드 루프 간격 idle은 캡처 버스트 아님");

    const std::vector<SessionRunPolicyInput> multiHold = {holdEngine, holdIdle};
    expectScenario(SessionRunPolicy::hasAnyCapturingWorkflowBurst(multiHold),
                   "multi_hold_any_burst",
                   u8"다중 홀드 중 하나라도 엔진 실행 시 버스트");

    SessionRunPolicyInput holdReleased;
    holdReleased.runningMode = FeatureRunMode::Hold;
    holdReleased.holdRunActive = false;
    holdReleased.repeatSession = false;
    holdReleased.engineRunning = false;
    expectScenario(!SessionRunPolicy::isSessionActive(holdReleased),
                   "hold_latch_released_inactive",
                   u8"홀드 latch 해제 후 비활성 세션");
}

void runMultiSessionRoadmapScenarios() {
    logLine("== Multi-session roadmap (R1.3) ==");

    const SessionRunPolicyInput watch = triggerWatch(false);
    const SessionRunPolicyInput holdEngine = holdActive(true);
    const std::vector<SessionRunPolicyInput> watchAndHold = {watch, holdEngine};
    expectScenario(!SessionRunPolicy::isCapturingWorkflowBurst(watch),
                   "trigger_watch_not_burst",
                   u8"트리거 감시 폴맼은 캡처 버스트 아님");
    expectScenario(SessionRunPolicy::hasAnyCapturingWorkflowBurst(watchAndHold),
                   "watch_plus_hold_burst",
                   u8"감시+홀드 엔진 시 버스트 존재");
    expectScenario(SessionRunPolicy::workflowRefreshOnProjectEdit(watch)
                       == WorkflowRefreshDecision::Immediate,
                   "watch_idle_refresh_while_hold_runs",
                   u8"감시 idle 세션은 홀드 실행 중에도 즉시 편집 허용");

    const SessionRunPolicyInput action = triggerAction(true);
    const std::vector<SessionRunPolicyInput> actionAndWatch = {action, triggerWatch(false)};
    expectScenario(SessionRunPolicy::hasAnyActiveWorkflowEngine(actionAndWatch),
                   "action_blocks_update_engine",
                   u8"트리거 동작 엔진 시 업데이트 차단");
    expectScenario(SessionRunPolicy::workflowRefreshOnProjectEdit(triggerWatch(false))
                       == WorkflowRefreshDecision::Immediate,
                   "watch_idle_refresh_during_action_on_other",
                   u8"다른 세션 동작 중 감시 idle 편집 즉시");

    const SessionRunPolicyInput repeatIdle = repeatInfinite(false);
    const SessionRunPolicyInput holdIdle = holdActive(false);
    const std::vector<SessionRunPolicyInput> three = {watch, holdIdle, repeatIdle};
    expectScenario(SessionRunPolicy::hasAnyRunningSession(three),
                   "background_profile_sessions_active",
                   u8"감시+홀드idle+무한idle 모두 활성 세션");
    const auto idx = SessionRunPolicy::activeWorkflowSessionIndices(three);
    expectScenario(idx.size() == 2 && idx[0] == 1 && idx[1] == 2,
                   "hold_and_repeat_workflow_while_watch_monitors",
                   u8"감시 중 홀드idle+무한idle 워크플로 활성 (감시는 감시만)");
}

void runForegroundGateScenarios() {
    logLine("== ForegroundRunGate scenarios ==");

    ForegroundRunGate::GateInput input;
    input.runWithoutTargetWindow = true;
    expectScenario(ForegroundRunGate::runForegroundGateActive(input),
                   "gate_run_without_target",
                   u8"타겟 없이 실행 시 포그라운드 게이트 통과");

    input.runWithoutTargetWindow = false;
    input.bindings.activeIsDefault = true;
    input.foregroundResolvedProfileId = QStringLiteral("default");
    input.foregroundResolvedIsDefault = true;
    expectScenario(ForegroundRunGate::foregroundProfileMatchesActive(input),
                   "gate_default_profile_match",
                   u8"기본 프로필 + 포그라운드 기본 매칭");

    input.bindings.activeIsDefault = false;
    input.bindings.activeProfileId = QStringLiteral("linked");
    input.foregroundResolvedProfileId = QStringLiteral("linked");
    input.foregroundResolvedIsDefault = false;
    expectScenario(ForegroundRunGate::foregroundProfileMatchesActive(input),
                   "gate_linked_profile_id_match",
                   u8"연동 프로필 ID 일치 시 매칭");

    ForegroundRunGate::FeatureGateOptions triggerOpts;
    triggerOpts.runMode = FeatureRunMode::Trigger;
    triggerOpts.triggerRunWithoutTargetForeground = true;
    input.feature = &triggerOpts;
    input.triggerBackgroundVisible = true;
    expectScenario(ForegroundRunGate::runForegroundGateActive(input),
                   "gate_trigger_background_visible",
                   u8"트리거 타겟 비포커스 감시 시 게이트 통과");

    input.feature = nullptr;
    input.foreground.rootHwnd = nullptr;
    expectScenario(!ForegroundRunGate::activeProfileForegroundBindingMatches(input),
                   "gate_no_hwnd_no_binding_match",
                   u8"포그라운드 HWND 없을 때 바인딩 불일치");
}

bool runInvariantSweeps() {
    logLine("== Invariant exhaustive sweep ==");
    const SessionRunPolicySweepResult exhaustive = runSessionRunPolicyExhaustiveSweep();
    logLine("  states checked: " + std::to_string(exhaustive.statesChecked));
    if (!exhaustive.violations.empty()) {
        for (const SessionRunPolicyViolation& v : exhaustive.violations) {
            std::cerr << "INVARIANT FAIL [" << v.invariantId << "]\n  " << v.messageKo << '\n';
            gReportLines.push_back("INVARIANT: " + v.invariantId);
        }
        return false;
    }

    logLine("== Invariant fuzz sweep (seed=0x8B19, n=4096) ==");
    const SessionRunPolicySweepResult fuzz = runSessionRunPolicyFuzzSweep(0x8B19u, 4096);
    logLine("  states checked: " + std::to_string(fuzz.statesChecked));
    if (!fuzz.violations.empty()) {
        for (const SessionRunPolicyViolation& v : fuzz.violations) {
            std::cerr << "FUZZ FAIL [" << v.invariantId << "]\n  " << v.messageKo << '\n';
            gReportLines.push_back("FUZZ: " + v.invariantId);
        }
        return false;
    }
    return true;
}

void writeReportFile(const char* path) {
    if (!path || !*path) {
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "WARN: could not write report: " << path << '\n';
        return;
    }
    for (const std::string& line : gReportLines) {
        out << line << '\n';
    }
    out << "scenario_passes=" << gScenarioPasses << '\n';
    out << "scenario_failures=" << gScenarioFailures << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const char* reportPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--report=", 0) == 0) {
            reportPath = argv[i] + 9;
        }
    }

    logLine("PIPBONG SessionRunPolicySim");
    runManualScenarios();
    runHoldBurstScenarios();
    runMultiSessionRoadmapScenarios();
    runForegroundGateScenarios();
    const bool invariantsOk = runInvariantSweeps();

    const int totalFailures = gScenarioFailures + (invariantsOk ? 0 : 1);
    if (totalFailures == 0) {
        logLine("OK: all scenarios and invariants passed.");
        writeReportFile(reportPath);
        return EXIT_SUCCESS;
    }

    logLine("FAILED: " + std::to_string(gScenarioFailures) + " scenario(s), invariants="
            + (invariantsOk ? "ok" : "FAIL"));
    writeReportFile(reportPath);
    return EXIT_FAILURE;
}
