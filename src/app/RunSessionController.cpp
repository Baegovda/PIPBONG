#include "RunSessionController.h"

#include "FeatureRunSession.h"
#include "model/Feature.h"
#include "model/Project.h"

#include <QTimer>

RunSessionController::RunSessionController(QObject* parent)
    : QObject(parent) {}

void RunSessionController::setProject(Project* project) {
    m_project = project;
}

void RunSessionController::setRunSessions(std::map<std::string, FeatureRunSession>* sessions) {
    m_runSessions = sessions;
}

void RunSessionController::setHostCallbacks(const HostCallbacks& callbacks) {
    m_host = callbacks;
}

void RunSessionController::reconcileWithForegroundGate() {
    if (!m_runSessions || !m_host.runForegroundGateActive) {
        return;
    }
    if (m_host.shouldSkipForegroundGateReconcile && m_host.shouldSkipForegroundGateReconcile()) {
        return;
    }

    bool needResumePoll = false;
    bool needRunUiRefresh = false;
    for (auto& entry : *m_runSessions) {
        FeatureRunSession& session = entry.second;
        if (session.userStopRequested) {
            continue;
        }

        Feature* feature = nullptr;
        if (m_host.featureForSession) {
            feature = m_host.featureForSession(session);
        } else if (m_project) {
            feature = m_project->featureById(session.featureId);
        }
        if (!feature) {
            continue;
        }

        const bool gateActive =
            m_host.runForegroundGateActiveForSession
                ? m_host.runForegroundGateActiveForSession(session, feature)
                : (m_host.runForegroundGateActive ? m_host.runForegroundGateActive(feature) : true);
        if (!gateActive) {
            if (!session.waitingForScopedTargetForeground) {
                session.waitingForScopedTargetForeground = true;
                needRunUiRefresh = true;
            }
            if (session.engine && session.engine->isRunning() && m_host.stopSessionEngine) {
                const bool holdHotkeyFirstLoop =
                    session.runningMode == FeatureRunMode::Hold && session.holdRunActive
                    && session.hotkeyLaunchedSession && session.sessionIteration == 0;
                if (!holdHotkeyFirstLoop) {
                    m_host.stopSessionEngine(session);
                }
            }
            needResumePoll = true;
        } else if (session.waitingForScopedTargetForeground) {
            session.waitingForScopedTargetForeground = false;
            needResumePoll = true;
            needRunUiRefresh = true;
        }
    }

    if (needRunUiRefresh && m_host.updateRunUiState) {
        m_host.updateRunUiState();
    }
    if (needResumePoll) {
        scheduleScopedTargetForegroundResumePoll();
    }
}

bool RunSessionController::deferRunUntilScopedTargetForeground(FeatureRunSession& session,
                                                               Feature* feature) {
    if (!m_host.runForegroundGateActive) {
        return false;
    }
    const bool gateActive =
        m_host.runForegroundGateActiveForSession
            ? m_host.runForegroundGateActiveForSession(session, feature)
            : (m_host.runForegroundGateActive ? m_host.runForegroundGateActive(feature) : true);
    if (gateActive) {
        session.waitingForScopedTargetForeground = false;
        return false;
    }
    session.waitingForScopedTargetForeground = true;
    scheduleScopedTargetForegroundResumePoll();
    return true;
}

void RunSessionController::scheduleScopedTargetForegroundResumePoll() {
    if (m_scopedTargetForegroundResumePending) {
        return;
    }
    m_scopedTargetForegroundResumePending = true;
    QTimer::singleShot(200, this, [this]() {
        m_scopedTargetForegroundResumePending = false;
        resumeWaitingScopedTargetForegroundSessions();
    });
}

void RunSessionController::resumeWaitingScopedTargetForegroundSessions() {
    if (!m_runSessions) {
        return;
    }

    bool stillWaiting = false;
    for (auto& entry : *m_runSessions) {
        FeatureRunSession& session = entry.second;
        if (!session.waitingForScopedTargetForeground) {
            continue;
        }

        Feature* feature = nullptr;
        if (m_host.featureForSession) {
            feature = m_host.featureForSession(session);
        } else if (m_project) {
            feature = m_project->featureById(session.featureId);
        }
        if (!feature) {
            session.waitingForScopedTargetForeground = false;
            continue;
        }
        const bool gateActive =
            m_host.runForegroundGateActiveForSession
                ? m_host.runForegroundGateActiveForSession(session, feature)
                : (m_host.runForegroundGateActive ? m_host.runForegroundGateActive(feature) : true);
        if (!gateActive) {
            stillWaiting = true;
            continue;
        }

        session.waitingForScopedTargetForeground = false;
        if (session.engine && session.engine->isRunning()) {
            continue;
        }

        if (session.runningMode == FeatureRunMode::Trigger) {
            if (session.triggerPhase == TriggerSessionPhase::Cooldown
                || session.triggerPhase == TriggerSessionPhase::RunningAction) {
                continue;
            }
            if (m_host.launchTriggerMonitor) {
                m_host.launchTriggerMonitor(session, feature, !session.triggerMonitorUiInitialized);
            }
            continue;
        }

        if (session.runningMode == FeatureRunMode::Hold) {
            if (!session.holdRunActive || !m_host.isHoldBindingDown
                || !m_host.isHoldBindingDown(session.featureId)) {
                continue;
            }
        }

        if (m_host.launchWorkflowRun) {
            m_host.launchWorkflowRun(session, feature, session.sessionIteration > 0);
        }
    }

    if (stillWaiting) {
        scheduleScopedTargetForegroundResumePoll();
    }
}

void RunSessionController::finishForegroundSessionGate(const FinishGateCallbacks& callbacks) {
    if (callbacks.switchingProfile && callbacks.switchingProfile()) {
        return;
    }
    if (callbacks.profileSwitchPipelineActive && callbacks.profileSwitchPipelineActive()) {
        return;
    }
    if (callbacks.flushDeferredProfileSwitchIfIdle) {
        callbacks.flushDeferredProfileSwitchIfIdle();
    }
    reconcileWithForegroundGate();
    resumeWaitingScopedTargetForegroundSessions();
    if (callbacks.scheduleEnsureTriggerMonitorEnginesRunning) {
        callbacks.scheduleEnsureTriggerMonitorEnginesRunning();
    }
    if (callbacks.updateTargetWindowDetails) {
        callbacks.updateTargetWindowDetails();
    }
}
