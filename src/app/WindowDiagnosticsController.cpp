#include "app/WindowDiagnosticsController.h"
#include "app/MainWindow.h"
#include "app/FeatureLibraryManager.h"
#include "app/MouseCenterLock.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/capture/ScreenCapture.h"
#include "model/Project.h"
#include "ui/FeatureListPanel.h"
#include "ui/AuxiliaryToolsController.h"
#include "ui/WindowShellController.h"
#include <QApplication>
#include <QDialog>
#include <QDateTime>
#include <QThread>
namespace {
QString triggerPhaseLabel(TriggerSessionPhase phase) {
    switch (phase) {
    case TriggerSessionPhase::Monitoring:
        return QStringLiteral("Monitoring");
    case TriggerSessionPhase::RunningAction:
        return QStringLiteral("RunningAction");
    case TriggerSessionPhase::Cooldown:
        return QStringLiteral("Cooldown");
    case TriggerSessionPhase::None:
    default:
        return QStringLiteral("None");
    }
}
QString libraryEntryDisplayName(FeatureLibraryManager* libraryManager, const QString& entryId) {
    if (!libraryManager) {
        return entryId;
    }
    for (const FeatureLibraryManager::Entry& entry : libraryManager->listEntries()) {
        if (entry.id == entryId) {
            return entry.name;
        }
    }
    return entryId;
}

}
WindowDiagnosticsController::WindowDiagnosticsController(MainWindow& window):QObject(&window),m_window(&window){
    const QPointer<WindowDiagnosticsController> life(this);
    m_registration=CrashReporter::setContextProvider([life]{return life?life->snapshot():QString();});
}
WindowDiagnosticsController::~WindowDiagnosticsController(){CrashReporter::clearContextProvider(m_registration);}
QString WindowDiagnosticsController::snapshot() const {
    const auto* app=QCoreApplication::instance();if(!app || QThread::currentThread()!=app->thread())return {};
    if(!m_window)return {};
    const auto& window=*m_window;
    QStringList lines;

    lines.append(QStringLiteral("  capturedAt: %1")
                     .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));

    if (window.m_windowShell && !window.m_windowShell->persistent().isEmpty()) {
        lines.append(QStringLiteral("  persistentStatus: %1").arg(window.m_windowShell->persistent()));
    }
    if (window.m_windowShell && !window.m_windowShell->transient().isEmpty()) {
        lines.append(QStringLiteral("  transientStatus: %1").arg(window.m_windowShell->transient()));
    }

    lines.append(QStringLiteral("  switchingProfile: %1")
                     .arg(window.m_switchingProfile ? QStringLiteral("yes") : QStringLiteral("no")));
    if (!window.m_profileSwitchCoordinator.deferredProfileSwitchId().isEmpty()) {
        lines.append(QStringLiteral("  deferredProfileSwitch: %1")
                         .arg(window.m_profileSwitchCoordinator.deferredProfileSwitchId()));
    }

    lines.append(QStringLiteral("  modelessTools: calculator=%1 memo=%2 spikeWatch=%3")
                     .arg(window.m_auxiliaryTools && window.m_auxiliaryTools->calculatorVisible() ? QStringLiteral("open")
                                                                                : QStringLiteral("closed"))
                     .arg(window.m_auxiliaryTools && window.m_auxiliaryTools->memoVisible() ? QStringLiteral("open")
                                                                    : QStringLiteral("closed"))
                     .arg(window.m_auxiliaryTools && window.m_auxiliaryTools->spikeWatchVisible() ? QStringLiteral("open")
                                                                                : QStringLiteral("closed")));
    lines.append(QStringLiteral("  mouseCenterLock: %1")
                     .arg(MouseCenterLock::isActive() ? QStringLiteral("active")
                                                      : QStringLiteral("inactive")));

    if (window.m_profileManager) {
        const QString profileId = window.m_profileManager->activeProfileId();
        QString profileName = profileId;
        if (const ProfileManager::Profile* profile = window.m_profileManager->activeProfile()) {
            profileName = profile->name;
        }
        lines.append(QStringLiteral("  profile: %1 (%2)").arg(profileName, profileId));
        lines.append(QStringLiteral("  defaultProfile: %1")
                         .arg(window.m_profileManager->isDefaultProfile(profileId) ? QStringLiteral("yes")
                                                                           : QStringLiteral("no")));
    }

    if (window.m_project) {
        const QString mainTarget = QString::fromStdString(window.m_project->targetWindowTitle());
        lines.append(QStringLiteral("  targetWindow: %1").arg(mainTarget.isEmpty() ? QStringLiteral("(empty)")
                                                                                  : mainTarget));
        if (window.m_profileManager) {
            const QString subTarget = window.m_profileManager->subTargetWindowTitle(window.m_profileManager->activeProfileId());
            if (!subTarget.isEmpty()) {
                lines.append(QStringLiteral("  subTargetWindow: %1").arg(subTarget));
            }
        }

        ScreenCapture::TargetWindowInfo targetInfo{};
#ifdef _WIN32
        HWND targetHwnd = ScreenCapture::targetWindow();
        if (!targetHwnd || !IsWindow(targetHwnd)) {
            targetHwnd = ScreenCapture::findTargetWindow();
        }
        const bool targetResolved =
            targetHwnd && ScreenCapture::queryWindowInfo(targetHwnd, targetInfo) && targetInfo.found;
#else
        const bool targetResolved =
            ScreenCapture::queryTargetWindowInfo(targetInfo) && targetInfo.found;
#endif
        if (targetResolved) {
            lines.append(QStringLiteral("  resolvedTarget: hwnd=0x%1 visible=%2 minimized=%3 client=%4x%5")
                             .arg(targetInfo.hwndValue, 0, 16)
                             .arg(targetInfo.visible ? QStringLiteral("yes") : QStringLiteral("no"))
                             .arg(targetInfo.minimized ? QStringLiteral("yes") : QStringLiteral("no"))
                             .arg(targetInfo.clientWidth)
                             .arg(targetInfo.clientHeight));
        } else {
            lines.append(QStringLiteral("  resolvedTarget: (not found)"));
        }
    }

    if (window.m_libraryPreviewFeature && !window.m_libraryPreviewEntryId.isEmpty()) {
        lines.append(QStringLiteral("  libraryPreview: %1 (%2)")
                         .arg(libraryEntryDisplayName(window.m_featureLibraryManager.get(), window.m_libraryPreviewEntryId),
                              window.m_libraryPreviewEntryId));
    } else if (window.m_featureList) {
        if (Feature* feature = window.m_featureList->selectedFeature()) {
            lines.append(QStringLiteral("  selectedFeature: %1 (%2)")
                             .arg(QString::fromStdString(feature->name()),
                                  QString::fromStdString(feature->id())));
            lines.append(QStringLiteral("  selectedFeatureScope: %1")
                             .arg(QString::fromStdString(
                                 featureCaptureTargetScopeToString(feature->captureTargetScope()))));
            lines.append(QStringLiteral("  selectedFeatureRequireScopedForeground: %1")
                             .arg(feature->requireScopedTargetForeground() ? QStringLiteral("yes")
                                                                         : QStringLiteral("no")));
        } else {
            lines.append(QStringLiteral("  selectedFeature: (none)"));
        }
    }

    lines.append(QStringLiteral("  runningSessions: %1").arg(static_cast<int>(window.runSessions().size())));
    lines.append(QStringLiteral("  abandonedEngines: %1").arg(static_cast<int>(window.m_abandonedEngines.size())));
    for (const auto& abandoned : window.m_abandonedEngines) {
        if (!abandoned) {
            continue;
        }
        lines.append(QStringLiteral("    - abandoned engine running=%1 liveWorker=%2")
                         .arg(abandoned->isRunning() ? QStringLiteral("yes") : QStringLiteral("no"))
                         .arg(abandoned->hasLiveWorker() ? QStringLiteral("yes") : QStringLiteral("no")));
    }

    const qint64 nowEpochMs = QDateTime::currentMSecsSinceEpoch();
    for (const auto& entry : window.runSessions()) {
        const FeatureRunSession& session = entry.second;
        const QString featureName=window.featureDisplayName(entry.first);
        const bool engineRunning = session.engine && session.engine->isRunning();
        const bool liveWorker = session.engine && session.engine->hasLiveWorker();
        QStringList sessionBits;
        sessionBits.append(QStringLiteral("owner=%1").arg(session.profileId));
        sessionBits.append(QStringLiteral("lane=%1/%2 roiWait=%3").arg(session.holdKeyTapLaneActive).arg(session.holdKeyTapLaneToken).arg(session.awaitingFirstTemplateRoi));
        sessionBits.append(QStringLiteral("mode=%1")
                               .arg(QString::fromStdString(featureRunModeToString(session.runningMode))));
        sessionBits.append(QStringLiteral("trigger=%1").arg(triggerPhaseLabel(session.triggerPhase)));
        sessionBits.append(QStringLiteral("block=#%1")
                               .arg(session.runningBlockIndex >= 0 ? session.runningBlockIndex + 1 : 0));
        sessionBits.append(QStringLiteral("engine=%1")
                               .arg(session.holdKeyTapLaneActive ? QStringLiteral("hold-tap") : engineRunning ? QStringLiteral("running")
                                                  : (liveWorker ? QStringLiteral("worker") : QStringLiteral("idle"))));
        sessionBits.append(QStringLiteral("hold=%1")
                               .arg(session.holdRunActive ? QStringLiteral("yes") : QStringLiteral("no")));
        sessionBits.append(QStringLiteral("userStop=%1")
                               .arg(session.userStopRequested ? QStringLiteral("yes") : QStringLiteral("no")));
        sessionBits.append(QStringLiteral("iteration=%1").arg(session.sessionIteration));
        if (session.repeatSession) {
            sessionBits.append(QStringLiteral("repeatRemaining=%1").arg(session.repeatRemaining));
        }
        if (!session.lockedCaptureTargetTitle.empty()) {
            sessionBits.append(QStringLiteral("capture=%1")
                                   .arg(QString::fromStdWString(session.lockedCaptureTargetTitle)));
        }
        if (session.waitingForScopedTargetForeground) {
            sessionBits.append(QStringLiteral("waitingScopedForeground=yes"));
        }
        if (session.deferredSessionWorkflowRefresh) {
            sessionBits.append(QStringLiteral("deferredWorkflowRefresh=yes"));
        }
        if (session.triggerPhase == TriggerSessionPhase::Cooldown && session.triggerCooldownEndsAtEpochMs > 0) {
            const qint64 remainingMs = session.triggerCooldownEndsAtEpochMs - nowEpochMs;
            sessionBits.append(QStringLiteral("cooldownRemainingMs=%1").arg(remainingMs));
        }
        if (session.consecutiveDetectionFailLoops > 0) {
            sessionBits.append(QStringLiteral("consecutiveDetectionFailLoops=%1")
                                   .arg(session.consecutiveDetectionFailLoops));
        }
        if (!session.triggerPreemptedSessions.empty()) {
            sessionBits.append(QStringLiteral("triggerPreempted=%1")
                                   .arg(static_cast<int>(session.triggerPreemptedSessions.size())));
        }
        if (session.sessionContext) {
            sessionBits.append(QStringLiteral("imageFindPoll=%1")
                                   .arg(session.sessionContext->imageFindPollAttempt()));
            sessionBits.append(QStringLiteral("stopRequested=%1")
                                   .arg(session.sessionContext->shouldStop() ? QStringLiteral("yes")
                                                                             : QStringLiteral("no")));
            sessionBits.append(QStringLiteral("paused=%1")
                                   .arg(session.sessionContext->isPaused() ? QStringLiteral("yes")
                                                                           : QStringLiteral("no")));
            const auto attempt=session.sessionContext->matchAttemptSnapshot();
            sessionBits.append(QStringLiteral("lastMatchConf=%1/%2").arg(attempt.confidence,0,'f',3).arg(attempt.threshold,0,'f',3));
        }
        lines.append(QStringLiteral("    - %1 %2").arg(featureName, sessionBits.join(QLatin1Char(' '))));

        if(session.runningBlockIndex>=0 && session.runningBlockSummaryIndex==session.runningBlockIndex && !session.runningBlockSummary.isEmpty())
            lines.append(QStringLiteral("      runningBlock: %1").arg(session.runningBlockSummary));
    }

    int visibleDialogs = 0;
    if (QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        const QWidgetList widgets = app->topLevelWidgets();
        for (QWidget* widget : widgets) {
            if (!widget || !widget->isVisible()) {
                continue;
            }
            if (qobject_cast<QDialog*>(widget)) {
                ++visibleDialogs;
                const QString title = widget->windowTitle().trimmed();
                lines.append(QStringLiteral("  dialog: %1 title=\"%2\"")
                                 .arg(QString::fromUtf8(widget->metaObject()->className()),
                                      title.isEmpty() ? QStringLiteral("(untitled)") : title));
            }
        }
        const QWidget* active = app->activeWindow();
        const bool pipbongForeground =
            active && (active == &window || active->window() == &window);
        lines.append(QStringLiteral("  pipbongForeground: %1")
                         .arg(pipbongForeground ? QStringLiteral("yes") : QStringLiteral("no")));
        if (active) {
            lines.append(QStringLiteral("  activeWindow: %1 title=\"%2\"")
                             .arg(QString::fromUtf8(active->metaObject()->className()),
                                  active->windowTitle().trimmed()));
        }
    }
    lines.append(QStringLiteral("  visibleDialogs: %1").arg(visibleDialogs));

    return lines.join(QLatin1Char('\n'));
}
