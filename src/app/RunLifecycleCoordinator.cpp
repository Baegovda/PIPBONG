#include "app/RunLifecycleCoordinator.h"

#include "app/ForegroundWindowMonitor.h"
#include "core/workflow/HoldKeyTapMultiplexer.h"
#include "app/MainWindow.h"
#include "app/FeatureRunSession.h"
#include "app/ProgramSettings.h"
#include "app/SessionRunPolicy.h"
#include "app/UserInputInterruptMonitor.h"
#include "core/capture/ScreenCapture.h"
#include "core/diagnostics/AppStutterProfiler.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/workflow/WorkflowRunner.h"
#include "model/Feature.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/FeatureRunMode.h"
#include "model/Project.h"
#include "ui/FeatureListPanel.h"
#include "ui/ProfileListWidget.h"
#include "ui/BlockListWidget.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/editors/WorkflowMatchFeedbackOverlay.h"
#include "ui/editors/WorkflowRoiFlashOverlay.h"
#include "ui/LogPanelWidget.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

bool holdSessionBlocksNewPhysicalStart(const FeatureRunSession& session,
                                       HoldKeyTapMultiplexer* mux) {
    if (session.engine && session.engine->isRunning()) {
        return true;
    }
    if (mux && session.holdKeyTapLaneActive && mux->isLaneActive(session.featureId)) {
        return true;
    }
    return false;
}

} // namespace

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

bool RunLifecycleCoordinator::isHoldBurstUiActive(const MainWindow& window) {
    return window.m_holdBurstDepth > 0
           || shouldCoalesceRunUi(window.m_runSessions, false) || window.m_holdStartUiFlushScheduled
           || window.m_holdEndCleanupScheduled || window.m_holdFeatureStartFlushScheduled
           || window.m_holdFeatureEndFinishFlushScheduled
           || !window.m_pendingHoldFeatureStartOrder.empty()
           || !window.m_pendingHoldFeatureEndFinishes.empty()
           || window.m_pendingHoldStartUiFeatureIds.size() > 1;
}

bool RunLifecycleCoordinator::shouldLogSessionDetailsInBurst(const MainWindow& window) {
    if (isHoldBurstUiActive(window)) {
        return false;
    }
    return window.m_runSessions.size() < 2;
}

void RunLifecycleCoordinator::prepareForegroundForHoldBurst(MainWindow& window) {
    if (window.m_holdBurstForegroundPrepared && isHoldBurstUiActive(window)) {
        return;
    }
    if (window.m_holdBurstForegroundPrepTimer.isValid()
        && window.m_holdBurstForegroundPrepTimer.elapsed() < 50) {
        return;
    }
#ifdef _WIN32
    window.ensureForegroundReadyForFeatureHotkey();
#endif
    window.m_holdBurstForegroundPrepTimer.start();
    window.m_holdBurstForegroundPrepared = true;
}

void RunLifecycleCoordinator::scheduleCoalescedHoldStartUi(MainWindow& window,
                                                             const std::string& featureId) {
    window.m_pendingHoldStartUiFeatureIds.insert(featureId);
    if (window.m_holdStartUiFlushScheduled) {
        return;
    }
    window.m_holdStartUiFlushScheduled = true;
    QTimer::singleShot(0, &window, [&window]() { flushCoalescedHoldStartUi(window); });
}

void RunLifecycleCoordinator::flushCoalescedHoldStartUi(MainWindow& window) {
    window.m_holdStartUiFlushScheduled = false;
    if (window.m_pendingHoldStartUiFeatureIds.empty()) {
        return;
    }

    const auto pending = window.m_pendingHoldStartUiFeatureIds;
    window.m_pendingHoldStartUiFeatureIds.clear();

    bool loggedStart = false;
    bool selectedDisplay = false;
    for (const std::string& featureId : pending) {
        FeatureRunSession* session = window.sessionFor(featureId);
        if (!session) {
            continue;
        }
        Feature* feature = window.m_project ? window.m_project->featureById(featureId) : nullptr;
        if (!selectedDisplay && feature) {
            window.selectRunningFeatureForDisplay(feature);
            selectedDisplay = true;
        }
        if (window.isDisplayedRunningFeature(session) && window.m_workflowEditor) {
            window.syncLoopTimingToWorkflowEditor(session);
            window.m_workflowEditor->clearBlockMatchResults();
            window.m_workflowEditor->clearExecutionHighlight();
            if (session->runningBlockIndex >= 0
                && session->runningBlockHighlight != BlockListWidget::ExecutionHighlight::None) {
                window.m_workflowEditor->setActiveBlockIndex(session->runningBlockIndex,
                                                             session->runningBlockHighlight);
            }
        }
        if (window.shouldLogRunDetails(*session) && !loggedStart) {
            window.appendSessionLog(*session, window.tr("기능 실행을 시작합니다"), LogLineKind::Accent);
            loggedStart = true;
        }
    }
    if (selectedDisplay && window.m_workflowEditor) {
        window.m_workflowEditor->persistRunFeedbackForCurrentFeature();
    }
    requestRunUiRefresh(window, false);
}

void RunLifecycleCoordinator::scheduleCoalescedHoldEndCleanup(MainWindow& window) {
    if (window.m_holdEndCleanupScheduled) {
        return;
    }
    window.m_holdEndCleanupScheduled = true;
    QTimer::singleShot(0, &window, [&window]() { flushCoalescedHoldEndCleanup(window); });
}

void RunLifecycleCoordinator::flushCoalescedHoldEndCleanup(MainWindow& window) {
    if (!window.m_holdEndCleanupScheduled) {
        return;
    }
    window.m_holdEndCleanupScheduled = false;
    window.reconcileMouseLocksFromRunningSessions();
    requestRunUiRefresh(window, false);
    flushDeferredBurstSideEffects(window);
}

void RunLifecycleCoordinator::scheduleHoldBurstScopeDrain(MainWindow& window) {
    if (window.m_holdBurstScopeDrainScheduled) {
        return;
    }
    window.m_holdBurstScopeDrainScheduled = true;
    QTimer::singleShot(0, &window, [&window]() { drainHoldBurstScope(window); });
}

void RunLifecycleCoordinator::drainHoldBurstScope(MainWindow& window) {
    window.m_holdBurstScopeDrainScheduled = false;
    if (!window.m_pendingHoldStartUiFeatureIds.empty() && !window.m_holdStartUiFlushScheduled) {
        flushCoalescedHoldStartUi(window);
    }
    flushDeferredBurstSideEffects(window);
}

void RunLifecycleCoordinator::flushDeferredBurstSideEffects(MainWindow& window) {
    if (window.m_deferredBurstTriggerRestore) {
        window.m_deferredBurstTriggerRestore = false;
        window.scheduleRestorePersistedTriggerSessions();
    }
    if (window.m_deferredBurstPruneEngines) {
        window.m_deferredBurstPruneEngines = false;
        window.schedulePruneAbandonedEngines();
    }
    window.m_holdBurstTargetActivated = false;
    window.m_holdBurstCaptureAppliedToCapture = false;
    if (window.m_holdBurstForegroundPrepTimer.isValid()
        && window.m_holdBurstForegroundPrepTimer.elapsed() > 200) {
        window.m_holdBurstCaptureTitleValid = false;
        window.m_holdBurstForegroundPrepared = false;
    }
}

void RunLifecycleCoordinator::scheduleCoalescedHoldFeatureStart(MainWindow& window,
                                                                const std::string& featureId) {
    if (window.m_pendingHoldFeatureStartIds.insert(featureId).second) {
        window.m_pendingHoldFeatureStartOrder.push_back(featureId);
    }
    if (window.m_holdFeatureStartFlushScheduled) {
        return;
    }
    window.m_holdFeatureStartFlushScheduled = true;
    QTimer::singleShot(0, &window, [&window]() { flushCoalescedHoldFeatureStarts(window); });
}

void RunLifecycleCoordinator::flushCoalescedHoldFeatureStarts(MainWindow& window) {
    window.m_holdFeatureStartFlushScheduled = false;
    if (window.m_pendingHoldFeatureStartOrder.empty()) {
        return;
    }

    std::vector<std::string> pending = std::move(window.m_pendingHoldFeatureStartOrder);
    window.m_pendingHoldFeatureStartOrder.clear();
    window.m_pendingHoldFeatureStartIds.clear();

    for (const std::string& featureId : pending) {
        Feature* feature = window.m_project ? window.m_project->featureById(featureId) : nullptr;
        if (feature && feature->enabled() && feature->runMode() == FeatureRunMode::Hold) {
            startFeatureRun(window, feature, true, false);
        }
    }
    window.scheduleFeatureListHoldVisualRefresh();
}

void RunLifecycleCoordinator::scheduleCoalescedHoldFeatureEndFinish(MainWindow& window,
                                                                    const std::string& featureId) {
    window.m_pendingHoldFeatureEndFinishes.insert(featureId);
    if (window.m_holdFeatureEndFinishFlushScheduled) {
        return;
    }
    window.m_holdFeatureEndFinishFlushScheduled = true;
    QTimer::singleShot(0, &window, [&window]() { flushCoalescedHoldFeatureEndFinishes(window); });
}

void RunLifecycleCoordinator::flushCoalescedHoldFeatureEndFinishes(MainWindow& window) {
    window.m_holdFeatureEndFinishFlushScheduled = false;
    if (window.m_pendingHoldFeatureEndFinishes.empty()) {
        return;
    }

    std::vector<std::string> pending(window.m_pendingHoldFeatureEndFinishes.begin(),
                                     window.m_pendingHoldFeatureEndFinishes.end());
    window.m_pendingHoldFeatureEndFinishes.clear();

    for (const std::string& featureId : pending) {
        if (window.sessionFor(featureId)) {
            finishRunSession(window, featureId, true, QString(), true);
        }
    }
    scheduleCoalescedHoldEndCleanup(window);
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
    if (immediate && shouldCoalesceRunUi(window.m_runSessions, false)) {
        immediate = false;
    }
    if (immediate) {
        QCoreApplication* app = QCoreApplication::instance();
        if (app && QThread::currentThread() != app->thread()) {
            qWarning("MainWindow: updateRunUiState(immediate) called off GUI thread");
            Q_ASSERT(false);
        }
        if (window.m_runUiDebounceTimer) {
            window.m_runUiDebounceTimer->stop();
        }
        applyRunUiState(window);
        return;
    }
    if (!window.m_runUiDebounceTimer) {
        applyRunUiState(window);
        return;
    }
    const auto policyInputs = policyInputsFromSessions(window.m_runSessions);
    const int intervalMs =
        SessionRunPolicy::runUiDebounceIntervalMs(policyInputs, window.m_runSessions.size());
    window.m_runUiDebounceTimer->setInterval(intervalMs);
    if (!window.m_runUiDebounceTimer->isActive()) {
        window.m_runUiDebounceTimer->start();
    }
}

void RunLifecycleCoordinator::applyRunUiState(MainWindow& window) {
    if (QCoreApplication::instance()) {
        Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    }
    window.reconcileHoldLatchForActiveHoldSessions();
    const bool burstUi = isHoldBurstUiActive(window) || window.shouldCoalesceRunUiUpdates();
    if (window.m_featureList) {
        bool suppressRunAnimation = false;
        for (const auto& entry : window.m_runSessions) {
            if (entry.second.sessionContext && entry.second.sessionContext->suppressRepeatUi()) {
                suppressRunAnimation = true;
                break;
            }
        }
        window.m_featureList->beginRunStateBatch();
        window.m_featureList->setRunAnimationLowCpu(suppressRunAnimation);
        window.m_featureList->setRunningFeatureIds(window.runningFeatureIds());
        QHash<QString, FeatureRunVisualKind> visualKinds = window.buildFeatureListRunVisualKinds();
        window.m_featureList->setFeatureRunVisualKinds(visualKinds);
        window.m_featureList->setActiveWorkflowFeatureIds(window.activeWorkflowFeatureIds());

        QHash<QString, FeatureTriggerCooldownState> cooldownStates;
        for (const auto& entry : window.m_runSessions) {
            if (!window.sessionBelongsToActiveProfile(entry.second)) {
                continue;
            }
            if (entry.second.runningMode != FeatureRunMode::Trigger
                || entry.second.triggerPhase != TriggerSessionPhase::Cooldown
                || entry.second.triggerCooldownEndsAtEpochMs <= 0) {
                continue;
            }
            FeatureTriggerCooldownState state;
            state.endsAtEpochMs = entry.second.triggerCooldownEndsAtEpochMs;
            state.totalMs = entry.second.triggerCooldownTotalMs;
            cooldownStates.insert(QString::fromStdString(entry.first), state);
        }
        window.m_featureList->setTriggerCooldownStates(cooldownStates);

        if (window.m_profileManager && window.m_project && !window.m_suppressTriggerArmedPersist) {
            const QString profileId = window.m_profileManager->activeProfileId();
            const QStringList armedIds = window.m_profileManager->triggerArmedFeatureIds(profileId);
            bool needRestore = false;
            for (const QString& armedId : armedIds) {
                if (visualKinds.contains(armedId)
                    || window.m_runSessions.find(armedId.toStdString()) != window.m_runSessions.end()) {
                    continue;
                }
                Feature* feature = window.m_project->featureById(armedId.toStdString());
                if (!feature || !feature->enabled()
                    || feature->runMode() != FeatureRunMode::Trigger) {
                    continue;
                }
                visualKinds.insert(armedId, FeatureRunVisualKind::TriggerWatch);
                needRestore = true;
            }
            if (needRestore) {
                if (burstUi) {
                    window.m_deferredBurstTriggerRestore = true;
                } else {
                    window.scheduleRestorePersistedTriggerSessions();
                }
            }
        }
        window.m_featureList->endRunStateBatch();
    }

    Feature* selected =
        window.m_featureList ? window.m_featureList->selectedFeature() : nullptr;
    const bool selectedRunning = selected && window.isFeatureRunning(selected->id());
    const bool selectedActiveWorkflow =
        selected && window.isFeatureInActiveWorkflowRun(selected->id());
    if (window.m_profileList) {
        window.m_profileList->setFeatureDropEnabled(true);
    }
    if (window.m_workflowEditor) {
        window.m_workflowEditor->setEditingEnabled(!window.m_libraryPreviewFeature
                                                   && !selectedActiveWorkflow);

        bool runBtnEnabled = false;
        bool showStop = false;
        QString disabledTip;
        if (selected && !window.m_libraryPreviewFeature) {
            showStop = selectedRunning;
            const bool holdMode = selected->runMode() == FeatureRunMode::Hold;
            const bool hasBlocks = !selected->workflow().blocks().empty();
            runBtnEnabled = selected->enabled() && (selectedRunning || (hasBlocks && !holdMode));
            if (holdMode && !selectedRunning) {
                disabledTip = window.tr(
                    "홀드 방식은 단축키를 누르고 있는 동안 워크플로가 무한 반복됩니다. 키를 떼면 중지됩니다.");
            } else if (!hasBlocks && !selectedRunning) {
                disabledTip = window.tr("선택한 기능에 블록이 없습니다.");
            } else if (!selected->enabled() && !selectedRunning) {
                disabledTip = window.tr("기능이 비활성화되어 있습니다.");
            }
        }
        window.m_workflowEditor->setRunStatusButtonState(showStop, runBtnEnabled, disabledTip);
    }

    AppStutterProfiler::setActiveFeatureSessionCount(
        static_cast<int>(window.m_runSessions.size()));
    AppStutterProfiler::setPipbongFeatureBurstActive(window.hasAnyActiveWorkflowEngine());

    if (window.hasAnyRunningSession()) {
        bool anyPaused = false;
        bool anyTriggerMonitoring = false;
        for (const auto& entry : window.m_runSessions) {
            if (entry.second.sessionContext && entry.second.sessionContext->isPaused()) {
                anyPaused = true;
            }
            if (entry.second.runningMode == FeatureRunMode::Trigger
                && entry.second.triggerPhase == TriggerSessionPhase::Monitoring) {
                anyTriggerMonitoring = true;
            }
        }
        if (anyPaused) {
            window.setPersistentStatus(window.tr("일시정지 — 입력하여 재개"));
        } else if (anyTriggerMonitoring) {
            window.setPersistentStatus(
                window.tr("트리거 감시 중 (%1)").arg(window.m_runSessions.size()));
        } else {
            window.setPersistentStatus(window.tr("실행 중 (%1)").arg(window.m_runSessions.size()));
        }
    } else {
        window.m_persistentStatusMessage.clear();
        if (window.m_transientStatusMessage.isEmpty()) {
            window.refreshTitleBarStatus();
        }
    }

    window.scheduleEnsureTriggerMonitorEnginesRunning();
    if (!burstUi) {
        window.schedulePruneAbandonedEngines();
        window.flushDeferredProfileSwitchIfIdle();
    } else {
        window.m_deferredBurstPruneEngines = true;
        if (window.m_runUiDebounceTimer && window.m_runUiDebounceTimer->isActive()) {
            window.m_runUiDebounceTimer->stop();
        }
    }
    window.maybeStartAutomaticUpdate();
}

RunLifecycleCoordinator::PreparedFeatureRunSession RunLifecycleCoordinator::prepareNewSession(
    MainWindow& window,
    Feature& feature,
    const QString& profileId,
    const bool fromHotkey,
    const bool skipTargetActivationOnStart) {
    PreparedFeatureRunSession prepared;
    prepared.session.featureId = feature.id();
    prepared.session.profileId = profileId;
    prepared.useHoldKeyTapFastPath =
        holdKeyTapWorkflowVirtualKey(feature, prepared.holdTapVirtualKey);
    if (!prepared.useHoldKeyTapFastPath) {
        prepared.session.engine = std::make_unique<WorkflowEngine>(&window);
    }
    prepared.session.userStopRequested = false;
    prepared.session.skipTargetActivationOnStart = skipTargetActivationOnStart;
    prepared.session.runningMode = feature.runMode();
    prepared.session.hotkeyLaunchedSession = fromHotkey;
    prepared.session.repeatSession =
        prepared.session.runningMode == FeatureRunMode::RepeatInfinite
        || prepared.session.runningMode == FeatureRunMode::RepeatCount
        || prepared.session.runningMode == FeatureRunMode::Hold
        || prepared.session.runningMode == FeatureRunMode::Trigger;
    prepared.session.repeatRemaining = feature.repeatCount();
    prepared.session.holdRunActive = prepared.session.runningMode == FeatureRunMode::Hold;
    if (prepared.session.runningMode == FeatureRunMode::Trigger) {
        prepared.session.triggerPhase = TriggerSessionPhase::Monitoring;
        prepared.session.triggerBlockIndex =
            WorkflowRunner::firstImageFindBlockIndex(feature.workflow());
    }
    if (prepared.session.runningMode == FeatureRunMode::Hold
        || prepared.session.runningMode == FeatureRunMode::RepeatInfinite
        || prepared.session.runningMode == FeatureRunMode::RepeatCount) {
        ++prepared.session.holdRepeatGeneration;
    }
    prepared.session.restoreMousePositionOnEnd = feature.restoreMousePositionOnEnd();
    prepared.session.lockMouseToScreenCenterDuringRun = feature.lockMouseToScreenCenterDuringRun();
    prepared.session.lockMouseToCurrentPositionDuringRun =
        feature.lockMouseToCurrentPositionDuringRun();
    prepared.session.lockMouseDuringFirstLoopCount = feature.lockMouseDuringFirstLoopCount();
    prepared.session.unlockMouseOnBlockFailureCount = feature.unlockMouseOnBlockFailureCount();

    if (!prepared.useHoldKeyTapFastPath) {
        window.connectSessionEngine(prepared.session);
    }
    return prepared;
}

RunLifecycleCoordinator::PreparedSessionLaunchOutcome
RunLifecycleCoordinator::activateAndLaunchPreparedSession(
    MainWindow& window,
    Feature* feature,
    FeatureRunSession& activeSession,
    const bool fromHotkey,
    const bool deferTriggerRestoreStart,
    const bool useHoldKeyTapFastPath,
    const int holdTapVirtualKey,
    const bool holdHotkeyStart) {
    if (!feature) {
        return PreparedSessionLaunchOutcome::Launched;
    }

    if (holdHotkeyStart && window.m_holdBurstCaptureTitleValid) {
        activeSession.lockedCaptureTargetTitle = window.m_holdBurstCaptureTitle;
        if (!window.m_holdBurstCaptureAppliedToCapture) {
            window.applySessionCaptureTarget(activeSession.lockedCaptureTargetTitle);
            window.m_holdBurstCaptureAppliedToCapture = true;
        }
    } else {
        activeSession.lockedCaptureTargetTitle = window.resolveRunCaptureTargetTitleW(feature);
        window.applySessionCaptureTarget(activeSession.lockedCaptureTargetTitle);
        if (holdHotkeyStart && !activeSession.lockedCaptureTargetTitle.empty()) {
            window.m_holdBurstCaptureTitle = activeSession.lockedCaptureTargetTitle;
            window.m_holdBurstCaptureTitleValid = true;
        }
    }

    if (window.m_runSessions.size() >= 2) {
        for (auto& entry : window.m_runSessions) {
            if (entry.second.sessionContext) {
                entry.second.sessionContext->setSuppressRepeatUi(true);
            }
        }
    }

    const bool hotkeyHoldStart = fromHotkey && feature->runMode() == FeatureRunMode::Hold;
    if (feature->runMode() == FeatureRunMode::Hold) {
        window.scheduleFeatureListHoldVisualRefresh();
    }
    if (!hotkeyHoldStart) {
        window.selectRunningFeatureForDisplay(feature);
    }

    if (window.tryBeginFirstTemplateRoiEdit(activeSession, feature)) {
        return PreparedSessionLaunchOutcome::AwaitingRoiEdit;
    }

    const std::string& featureId = activeSession.featureId;
    if (feature->runMode() == FeatureRunMode::Trigger) {
        window.persistTriggerArmedState(QString::fromStdString(featureId), true);
        if (deferTriggerRestoreStart) {
            activeSession.waitingForScopedTargetForeground = true;
            window.m_runSessionController.scheduleScopedTargetForegroundResumePoll();
            window.updateRunUiState();
            return PreparedSessionLaunchOutcome::Launched;
        }
        window.launchTriggerMonitor(activeSession, feature, true);
        return PreparedSessionLaunchOutcome::Launched;
    }

    if (useHoldKeyTapFastPath) {
        activeSession.usesHoldKeyTapFastPath = true;
        window.launchHoldKeyTapRun(activeSession, feature, holdTapVirtualKey);
        return PreparedSessionLaunchOutcome::Launched;
    }

    window.launchWorkflowRun(activeSession, feature, false);
    return PreparedSessionLaunchOutcome::Launched;
}

void RunLifecycleCoordinator::startFeatureRun(MainWindow& window,
                                              Feature* feature,
                                              const bool fromHotkey,
                                              const bool skipTargetActivationOnStart) {
    if (!feature) {
        return;
    }
    if (!feature->enabled()) {
        return;
    }
    const bool silentRestoreStart = window.m_suppressTriggerArmedPersist;
    if (feature->workflow().blocks().empty()) {
        if (!silentRestoreStart) {
            QMessageBox::information(
                &window, window.tr("실행"), window.tr("선택한 기능에 블록이 없습니다."));
        }
        return;
    }
    if (feature->runMode() == FeatureRunMode::Trigger
        && WorkflowRunner::firstImageFindBlockIndex(feature->workflow()) < 0) {
        if (!silentRestoreStart) {
            QMessageBox::information(
                &window,
                window.tr("실행"),
                window.tr("트리거 모드에는 템플릿이 지정된 템플릿 매칭 블록이 최소 하나 필요합니다."));
        }
        return;
    }
    bool deferTriggerRestoreStart = false;
    const bool holdHotkeyStart = fromHotkey && feature->runMode() == FeatureRunMode::Hold;
    if (holdHotkeyStart) {
        prepareForegroundForHoldBurst(window);
    }
    switch (evaluateRunStartForeground(
            window, feature, fromHotkey, silentRestoreStart, holdHotkeyStart)) {
    case RunStartForegroundOutcome::Proceed:
        break;
    case RunStartForegroundOutcome::DeferTriggerRestore:
        deferTriggerRestoreStart = true;
        break;
    case RunStartForegroundOutcome::Abort:
        return;
    }

    const std::string featureId = feature->id();
    switch (reconcileExistingSessionBeforeStart(window, featureId, holdHotkeyStart)) {
    case ExistingSessionReconcileOutcome::AbortAlreadyActive:
        return;
    case ExistingSessionReconcileOutcome::NoExistingSession:
    case ExistingSessionReconcileOutcome::StaleRemoved:
        break;
    }

    const QString profileId =
        window.m_profileManager ? window.m_profileManager->activeProfileId() : QString();
    PreparedFeatureRunSession prepared =
        prepareNewSession(window, *feature, profileId, fromHotkey, skipTargetActivationOnStart);
    const bool useHoldKeyTapFastPath = prepared.useHoldKeyTapFastPath;
    const int holdTapVirtualKey = prepared.holdTapVirtualKey;

    window.m_runSessions.emplace(featureId, std::move(prepared.session));
    if (!silentRestoreStart && window.m_runSessions.size() >= 4) {
        window.showTransientStatus(
            window.tr("동시에 여러 기능이 실행 중입니다. 화면이 버벅일 수 있습니다."),
            4500);
    }
    FeatureRunSession& activeSession = window.m_runSessions.at(featureId);
    activateAndLaunchPreparedSession(
        window,
        feature,
        activeSession,
        fromHotkey,
        deferTriggerRestoreStart,
        useHoldKeyTapFastPath,
        holdTapVirtualKey,
        holdHotkeyStart);
}

RunLifecycleCoordinator::ExistingSessionReconcileOutcome
RunLifecycleCoordinator::reconcileExistingSessionBeforeStart(MainWindow& window,
                                                             const std::string& featureId,
                                                             const bool holdHotkeyStart) {
    FeatureRunSession* existing = window.sessionFor(featureId);
    if (!existing) {
        return ExistingSessionReconcileOutcome::NoExistingSession;
    }
    const bool staleHoldStart =
        holdHotkeyStart
        && !holdSessionBlocksNewPhysicalStart(*existing, window.m_holdKeyTapMux);
    if (!staleHoldStart && window.isFeatureSessionActive(*existing)) {
        return ExistingSessionReconcileOutcome::AbortAlreadyActive;
    }
    if (existing->sessionContext) {
        existing->sessionContext->endRunInputSession();
    }
    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    if (existing->holdKeyTapLaneActive && window.m_holdKeyTapMux) {
        window.m_holdKeyTapMux->stopLane(featureId);
        existing->holdKeyTapLaneActive = false;
    }
    if (existing->engine) {
        window.abandonSessionEngine(*existing);
    }
    window.m_runSessions.erase(featureId);
    return ExistingSessionReconcileOutcome::StaleRemoved;
}

void RunLifecycleCoordinator::stopFeatureRun(MainWindow& window, const std::string& featureId) {
    FeatureRunSession* session = window.sessionFor(featureId);
    if (!session) {
        return;
    }

    CrashReporter::noteBreadcrumb(QStringLiteral("run"),
                                  QStringLiteral("stop feature %1")
                                      .arg(window.featureDisplayName(featureId)));

    applyUserStopRequestFlags(*session, window.m_suppressTriggerArmedPersist);
    if (session->disarmPersistedTrigger) {
        window.persistTriggerArmedState(QString::fromStdString(featureId), false);
    }

    Feature* feature = window.m_project ? window.m_project->featureById(featureId) : nullptr;
    if (session->runningMode == FeatureRunMode::Hold) {
        window.releaseHoldHotkeyInputToTarget(*session, feature);
    }
    UserInputInterruptMonitor::instance().unregisterSession(featureId);

    const bool hadEngine = session->engine != nullptr;
    const bool hadHoldTapLane = session->holdKeyTapLaneActive;
    if (hadHoldTapLane && window.m_holdKeyTapMux) {
        window.m_holdKeyTapMux->stopLane(featureId);
        return;
    }
    if (hadEngine) {
        window.appendSessionLog(*session,
                                window.tr("실행 중지를 요청했습니다."),
                                LogLineKind::Warning);
        window.abandonSessionEngine(*session);
        if (window.shouldCoalesceRunUiUpdates()) {
            scheduleCoalescedHoldEndCleanup(window);
        } else {
            window.reconcileMouseLocksFromRunningSessions();
            requestRunUiRefresh(window, false);
        }
        window.schedulePruneAbandonedEngines();
        return;
    }

    finishRunSession(window,
                     featureId,
                     session->lastLoopSuccess,
                     QString(),
                     window.shouldCoalesceRunUiUpdates());
    if (window.shouldCoalesceRunUiUpdates()) {
        scheduleCoalescedHoldEndCleanup(window);
    } else {
        requestRunUiRefresh(window, false);
    }
}

void RunLifecycleCoordinator::finishRunSession(MainWindow& window,
                                               const std::string& featureId,
                                               const bool success,
                                               const QString& message,
                                               const bool deferUiUpdate) {
    FeatureRunSession* session = window.sessionFor(featureId);
    const bool coalesceUi = shouldCoalesceRunUi(window.m_runSessions, deferUiUpdate);
    if (session && window.isDisplayedRunningFeature(session) && !coalesceUi) {
        window.m_workflowEditor->clearExecutionHighlight();
        window.m_workflowEditor->persistRunFeedbackForCurrentFeature();
    }

    if (session && !coalesceUi) {
        window.showTransientStatus(
            window.tr("[%1] %2")
                .arg(window.featureDisplayName(featureId), success ? window.tr("완료") : window.tr("실패")),
            3000);
    }

    if (session && session->runningMode == FeatureRunMode::Hold) {
        Feature* feature = window.featureForSession(*session);
        window.releaseHoldHotkeyInputToTarget(*session, feature);
    }

    if (session && session->sessionContext) {
        session->sessionContext->endRunInputSession();
    }

    if (session && session->runningMode == FeatureRunMode::Trigger) {
        window.resumePreemptedSessionsForTrigger(*session);
    }

    if (session) {
        ++session->triggerCooldownGeneration;
        ++session->triggerMonitorRestartGeneration;
        ++session->holdRepeatGeneration;
        window.restoreRunStartCursorPosition(*session);
    }

    if (session && session->runningMode == FeatureRunMode::Trigger && session->disarmPersistedTrigger) {
        if (window.m_profileManager && !session->profileId.isEmpty()) {
            window.m_profileManager->updateTriggerArmedFeature(session->profileId,
                                                               QString::fromStdString(featureId),
                                                               false);
        } else {
            window.persistTriggerArmedState(QString::fromStdString(featureId), false);
        }
    }

    if (session && session->engine) {
        window.abandonSessionEngine(*session);
    }

    if (session && session->holdKeyTapLaneActive && window.m_holdKeyTapMux) {
        window.m_holdKeyTapMux->stopLane(featureId);
        session->holdKeyTapLaneActive = false;
    }

    for (auto it = window.m_abandonedEngineFeatureIds.begin();
         it != window.m_abandonedEngineFeatureIds.end();) {
        if (it->second == featureId) {
            it = window.m_abandonedEngineFeatureIds.erase(it);
        } else {
            ++it;
        }
    }

    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    window.m_fastRepeatUiCoalesce.erase(featureId);
    window.m_runSessions.erase(featureId);
    window.pruneSessionOwnerProjects();
    if (!coalesceUi) {
        window.reconcileMouseLocksFromRunningSessions();
    }
    if (!window.hasAnyRunningSession()) {
        WorkflowMatchFeedbackOverlay::dismissAll();
        WorkflowRoiFlashOverlay::dismissAll();
    }
    if (!deferUiUpdate) {
        window.updateRunUiState(false);
    } else {
        scheduleCoalescedHoldEndCleanup(window);
    }
    Q_UNUSED(message);
}
