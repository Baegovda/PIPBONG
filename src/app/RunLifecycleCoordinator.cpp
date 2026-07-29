#include "app/RunLifecycleCoordinator.h"

#include "app/ForegroundWindowMonitor.h"
#include "app/MainWindow.h"
#include "app/FeatureRunSession.h"
#include "app/ProgramSettings.h"
#include "app/SessionRunPolicy.h"
#include "core/capture/ScreenCapture.h"
#include "model/Feature.h"
#include "model/FeatureCaptureTargetScope.h"

#include <QMessageBox>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

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

FeatureRunSession* RunLifecycleCoordinator::sessionForId(
    std::map<std::string, FeatureRunSession>& sessions,
    const std::string& featureId) {
    const auto it = sessions.find(featureId);
    return it == sessions.end() ? nullptr : &it->second;
}

const FeatureRunSession* RunLifecycleCoordinator::sessionForId(
    const std::map<std::string, FeatureRunSession>& sessions,
    const std::string& featureId) {
    const auto it = sessions.find(featureId);
    return it == sessions.end() ? nullptr : &it->second;
}

bool RunLifecycleCoordinator::shouldContinueSession(const FeatureRunSession& session,
                                                    bool holdBindingStillActive) {
    if (session.userStopRequested) {
        return false;
    }
    return SessionRunPolicy::shouldContinueSession(policyInputFrom(session), holdBindingStillActive);
}

bool RunLifecycleCoordinator::shouldCoalesceRunUi(
    const std::map<std::string, FeatureRunSession>& sessions,
    const bool deferUiUpdate) {
    if (deferUiUpdate) {
        return true;
    }
    return SessionRunPolicy::shouldCoalesceRunUiUpdates(policyInputsFromSessions(sessions));
}

bool RunLifecycleCoordinator::shouldDeferAbandonedEnginePrune(
    const std::map<std::string, FeatureRunSession>& sessions,
    const bool holdBurstUiActive) {
    if (holdBurstUiActive) {
        return true;
    }
    return SessionRunPolicy::shouldCoalesceRunUiUpdates(policyInputsFromSessions(sessions));
}

void RunLifecycleCoordinator::applyUserStopRequestFlags(FeatureRunSession& session,
                                                          const bool suppressTriggerArmedPersist) {
    session.userStopRequested = true;
    session.repeatSession = false;
    session.holdRunActive = false;
    session.waitingForScopedTargetForeground = false;
    ++session.holdRepeatGeneration;
    ++session.triggerCooldownGeneration;
    ++session.triggerMonitorRestartGeneration;

    if (session.runningMode == FeatureRunMode::Trigger) {
        session.disarmPersistedTrigger = !suppressTriggerArmedPersist;
    }
}

RunLifecycleCoordinator::RunStartForegroundOutcome
RunLifecycleCoordinator::evaluateRunStartForeground(MainWindow& window,
                                                    Feature* feature,
                                                    const bool fromHotkey,
                                                    const bool silentRestoreStart,
                                                    const bool holdHotkeyStart) {
    if (!feature) {
        return RunStartForegroundOutcome::Abort;
    }
#ifdef _WIN32
    if (!ProgramSettings::runWithoutTargetWindow()) {
        const bool skipHeavyForegroundSync =
            holdHotkeyStart && window.m_holdBurstForegroundPrepared && window.isHoldBurstActive();
        if (!skipHeavyForegroundSync) {
            window.switchToForegroundLinkedProfileIfNeeded(true);
            if (!fromHotkey) {
                window.applyProfileSwitchFromForegroundState(
                    window.m_foregroundMonitor->currentState());
            }
            window.syncEffectiveTargetWindowTitleToCapture();
            if (fromHotkey && !holdHotkeyStart) {
                QTimer::singleShot(0, &window, [&window]() {
                    window.reconcileRunSessionsWithForegroundGate();
                });
            } else if (!fromHotkey) {
                window.reconcileRunSessionsWithForegroundGate();
            }
        }
        const bool foregroundGateActive = window.runForegroundGateActive(feature);
        const std::wstring captureTitle = window.resolveRunCaptureTargetTitleW(feature);
        const FeatureCaptureTargetScope scope = feature->captureTargetScope();
        const bool triggerRestore =
            silentRestoreStart && feature->runMode() == FeatureRunMode::Trigger;
        if (!foregroundGateActive) {
            if (!silentRestoreStart) {
                QMessageBox::information(
                    &window,
                    window.tr("실행"),
                    window.tr("현재 포커스 창에 연결된 프로필의 기능만 실행됩니다. "
                                "해당 프로필로 전환하거나 타겟 프로그램을 활성화한 뒤 다시 시도하세요."));
                return RunStartForegroundOutcome::Abort;
            }
            if (triggerRestore) {
                return RunStartForegroundOutcome::DeferTriggerRestore;
            }
            return RunStartForegroundOutcome::Abort;
        }
        if (captureTitle.empty()) {
            if (!silentRestoreStart) {
                QString message;
                if (scope == FeatureCaptureTargetScope::SubOnly) {
                    message = window.tr("이 기능은 서브 창에서만 실행됩니다. 프로필 편집에서 서브 창을 "
                                        "지정하세요.");
                } else {
                    message = window.tr("타겟이 지정되지 않았습니다. '타겟 지정'으로 타겟을 선택하거나, "
                                        "프로그램 설정에서 '창을 지정하지 않은 상태에서도 동작'을 켜세요.");
                }
                QMessageBox::information(&window, window.tr("실행"), message);
                return RunStartForegroundOutcome::Abort;
            }
            if (triggerRestore) {
                return RunStartForegroundOutcome::DeferTriggerRestore;
            }
            return RunStartForegroundOutcome::Abort;
        } else {
            std::wstring processPath;
            if (window.m_profileManager && !window.isActiveDefaultProfile()) {
                const QString profileId = window.m_profileManager->activeProfileId();
                if (scope == FeatureCaptureTargetScope::SubOnly) {
                    processPath =
                        window.m_profileManager->subLinkedTargetProcessPath(profileId).toStdWString();
                } else {
                    processPath =
                        window.m_profileManager->linkedTargetProcessPath(profileId).toStdWString();
                }
            }
            bool captureHwndValid = false;
            if (holdHotkeyStart && window.m_holdBurstCaptureTitleValid
                && captureTitle == window.m_holdBurstCaptureTitle) {
                captureHwndValid = true;
            } else {
                const HWND captureHwnd =
                    ScreenCapture::findVisibleWindowMatchingTitle(captureTitle, processPath);
                captureHwndValid = captureHwnd != nullptr;
                if (captureHwndValid && holdHotkeyStart) {
                    window.m_holdBurstCaptureTitle = captureTitle;
                    window.m_holdBurstCaptureTitleValid = true;
                }
            }
            if (!captureHwndValid) {
                if (!silentRestoreStart) {
                    QString message;
                    if (scope == FeatureCaptureTargetScope::SubOnly) {
                        message = window.tr("서브 창을 찾을 수 없습니다. 해당 창이 실행 중인지 확인하세요.");
                    } else if (scope == FeatureCaptureTargetScope::MainOnly) {
                        message = window.tr("메인 창을 찾을 수 없습니다. '타겟 지정'으로 타겟을 선택하거나, "
                                            "프로그램 설정에서 '창을 지정하지 않은 상태에서도 동작'을 켜세요.");
                    } else {
                        message = window.tr("타겟을 찾을 수 없습니다. 메인·서브 창이 실행 중인지 확인하거나, "
                                            "'타겟 지정'·프로필 편집을 확인하세요.");
                    }
                    QMessageBox::information(&window, window.tr("실행"), message);
                    return RunStartForegroundOutcome::Abort;
                }
                if (triggerRestore) {
                    return RunStartForegroundOutcome::DeferTriggerRestore;
                }
                return RunStartForegroundOutcome::Abort;
            }
        }
    }
#endif
    return RunStartForegroundOutcome::Proceed;
}

void RunLifecycleCoordinator::requestRunUiRefresh(MainWindow& window, bool immediate) {
    window.updateRunUiState(immediate);
}
