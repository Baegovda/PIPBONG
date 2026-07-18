#include "app/SessionRunPolicyInvariants.h"

#include <array>
#include <cstdint>
#include <random>

namespace {

void addViolation(std::vector<SessionRunPolicyViolation>& out,
                  const char* id,
                  const char* messageKo,
                  const char* detailEn) {
    out.push_back(SessionRunPolicyViolation{id, messageKo, detailEn});
}

std::vector<SessionRunPolicyViolation> checkInvariants(const SessionRunPolicyInput& session) {
    std::vector<SessionRunPolicyViolation> violations;

    const bool active = SessionRunPolicy::isSessionActive(session);
    const bool workflowActive = SessionRunPolicy::isInActiveWorkflowRun(session);
    const bool earlyLock = SessionRunPolicy::isEarlyLoopMouseLockWindow(session);
    const WorkflowRefreshDecision refresh = SessionRunPolicy::workflowRefreshOnProjectEdit(session);

    if (workflowActive && !active) {
        addViolation(violations,
                     "active_workflow_implies_active_session",
                     u8"워크플로 실행 중인데 세션이 비활성으로 판정됨",
                     "isInActiveWorkflowRun true but isSessionActive false");
    }

    if (session.runningMode == FeatureRunMode::Trigger) {
        if (workflowActive && session.triggerPhase != TriggerSessionPhase::RunningAction) {
            addViolation(violations,
                         "trigger_workflow_only_on_action",
                         u8"트리거 감시/쿨다운인데 워크플로 실행 중으로 판정됨",
                         "trigger workflow active outside RunningAction");
        }
        if (!workflowActive && session.triggerPhase == TriggerSessionPhase::RunningAction && active) {
            addViolation(violations,
                         "trigger_action_is_workflow",
                         u8"트리거 동작 단계인데 워크플로 실행 중이 아님",
                         "RunningAction with active session but not active workflow");
        }
    }

    if (earlyLock) {
        if (session.lockMouseDuringFirstLoopCount <= 0) {
            addViolation(violations,
                         "early_lock_requires_setting",
                         u8"초기 루프 마우스 잠금이 켜졌는데 설정값이 0 이하",
                         "early lock window without lockMouseDuringFirstLoopCount");
        }
        if (session.earlyLoopMouseLockReleased) {
            addViolation(violations,
                         "early_lock_not_when_released",
                         u8"초기 루프 잠금 해제 후에도 잠금 구간으로 판정됨",
                         "early lock window after earlyLoopMouseLockReleased");
        }
        if (session.runningMode == FeatureRunMode::Trigger
            && session.triggerPhase != TriggerSessionPhase::RunningAction) {
            addViolation(violations,
                         "early_lock_not_during_trigger_watch",
                         u8"트리거 감시/쿨다운 중 초기 루프 마우스 잠금이 켜짐",
                         "early lock during trigger non-action phase");
        }
    }

    if (!active && refresh != WorkflowRefreshDecision::SkipInactive) {
        addViolation(violations,
                     "refresh_skip_when_inactive",
                     u8"비활성 세션인데 워크플로 편집 반영 정책이 SkipInactive가 아님",
                     "inactive session with non-SkipInactive refresh");
    }
    if (active && session.engineRunning && refresh != WorkflowRefreshDecision::Defer) {
        addViolation(violations,
                     "refresh_defer_when_engine",
                     u8"엔진 실행 중인데 워크플로 편집이 즉시 반영됨",
                     "engine running but refresh is not Defer");
    }
    if (active && !session.engineRunning && refresh != WorkflowRefreshDecision::Immediate) {
        addViolation(violations,
                     "refresh_immediate_when_idle_engine",
                     u8"엔진이 멈춘 활성 세션인데 워크플로 편집이 지연됨",
                     "active session with idle engine but refresh is not Immediate");
    }

    if (session.engineRunning && !active) {
        addViolation(violations,
                     "engine_implies_active_session",
                     u8"엔진이 도는데 세션이 비활성으로 판정됨",
                     "engineRunning true but isSessionActive false");
    }

    const std::vector<SessionRunPolicyInput> single{session};
    if (session.engineRunning != SessionRunPolicy::hasAnyActiveWorkflowEngine(single)) {
        addViolation(violations,
                     "aggregate_engine_matches_input",
                     u8"업데이트 차단(엔진 실행) 집계 결과가 입력과 불일치",
                     "hasAnyActiveWorkflowEngine mismatch for single session");
    }
    if (active != SessionRunPolicy::hasAnyRunningSession(single)) {
        addViolation(violations,
                     "aggregate_session_matches_input",
                     u8"세션 활성 집계 결과가 입력과 불일치",
                     "hasAnyRunningSession mismatch for single session");
    }

    const auto workflowIndices = SessionRunPolicy::activeWorkflowSessionIndices(single);
    const bool indexListsWorkflow = !workflowIndices.empty();
    if (workflowActive != indexListsWorkflow) {
        addViolation(violations,
                     "active_workflow_index_consistent",
                     u8"활성 워크플로 목록과 단일 세션 판정이 불일치",
                     "activeWorkflowSessionIndices disagrees with isInActiveWorkflowRun");
    }

    return violations;
}

SessionRunPolicyInput makeInput(FeatureRunMode mode,
                                TriggerSessionPhase phase,
                                bool repeatSession,
                                bool holdRunActive,
                                bool engineRunning,
                                int lockLoops,
                                bool earlyReleased,
                                int runLoopNumber) {
    SessionRunPolicyInput input;
    input.runningMode = mode;
    input.triggerPhase = phase;
    input.repeatSession = repeatSession;
    input.holdRunActive = holdRunActive;
    input.engineRunning = engineRunning;
    input.lockMouseDuringFirstLoopCount = lockLoops;
    input.earlyLoopMouseLockReleased = earlyReleased;
    input.runLoopNumber = runLoopNumber;
    input.sessionIteration = runLoopNumber > 0 ? runLoopNumber - 1 : 0;
    input.repeatRemaining = repeatSession ? 2 : 0;
    return input;
}

constexpr std::array<FeatureRunMode, 4> kModes = {
    FeatureRunMode::Hold,
    FeatureRunMode::RepeatInfinite,
    FeatureRunMode::RepeatCount,
    FeatureRunMode::Trigger,
};

constexpr std::array<TriggerSessionPhase, 4> kPhases = {
    TriggerSessionPhase::None,
    TriggerSessionPhase::Monitoring,
    TriggerSessionPhase::RunningAction,
    TriggerSessionPhase::Cooldown,
};

bool isPlausibleInput(const SessionRunPolicyInput& input) {
    if (input.runningMode != FeatureRunMode::Trigger
        && input.triggerPhase != TriggerSessionPhase::None) {
        return false;
    }
    if (input.runningMode == FeatureRunMode::Hold && !input.holdRunActive && !input.engineRunning
        && !input.repeatSession) {
        return false;
    }
    return true;
}

} // namespace

std::vector<SessionRunPolicyViolation> checkSessionRunPolicyInvariants(
    const SessionRunPolicyInput& session) {
    return checkInvariants(session);
}

SessionRunPolicySweepResult runSessionRunPolicyExhaustiveSweep() {
    SessionRunPolicySweepResult result;

    const std::array<bool, 2> flags{false, true};
    const std::array<int, 3> lockValues{0, 2, 5};
    const std::array<int, 3> loopNumbers{0, 1, 4};

    for (FeatureRunMode mode : kModes) {
        for (TriggerSessionPhase phase : kPhases) {
            for (bool repeatSession : flags) {
                for (bool holdRunActive : flags) {
                    for (bool engineRunning : flags) {
                        for (int lockLoops : lockValues) {
                            for (bool earlyReleased : flags) {
                                for (int runLoop : loopNumbers) {
                                    SessionRunPolicyInput input =
                                        makeInput(mode,
                                                  phase,
                                                  repeatSession,
                                                  holdRunActive,
                                                  engineRunning,
                                                  lockLoops,
                                                  earlyReleased,
                                                  runLoop);
                                    if (!isPlausibleInput(input)) {
                                        continue;
                                    }
                                    ++result.statesChecked;
                                    auto violations = checkInvariants(input);
                                    result.violations.insert(result.violations.end(),
                                                             violations.begin(),
                                                             violations.end());
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

SessionRunPolicySweepResult runSessionRunPolicyFuzzSweep(std::uint32_t seed, int sampleCount) {
    SessionRunPolicySweepResult result;
    std::mt19937 rng(seed);

    for (int i = 0; i < sampleCount; ++i) {
        SessionRunPolicyInput input;
        input.runningMode = kModes[rng() % kModes.size()];
        input.triggerPhase = kPhases[rng() % kPhases.size()];
        input.repeatSession = (rng() & 1) != 0;
        input.holdRunActive = (rng() & 1) != 0;
        input.engineRunning = (rng() & 1) != 0;
        input.lockMouseDuringFirstLoopCount = static_cast<int>(rng() % 8);
        input.earlyLoopMouseLockReleased = (rng() & 1) != 0;
        input.runLoopNumber = static_cast<int>(rng() % 10);
        input.sessionIteration = static_cast<int>(rng() % 10);
        input.repeatRemaining = static_cast<int>(rng() % 5);

        if (!isPlausibleInput(input)) {
            continue;
        }

        ++result.statesChecked;
        auto violations = checkInvariants(input);
        result.violations.insert(result.violations.end(), violations.begin(), violations.end());
    }

    return result;
}
