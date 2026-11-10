#include "ProfileSwitchCoordinator.h"

#include "core/diagnostics/CrashReporter.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProfileForegroundResolver.h"
#include "app/ProfileManager.h"
#include "ui/widgets/ReorderableListWidget.h"

#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

ProfileSwitchCoordinator::ProfileSwitchCoordinator(QObject* parent)
    : QObject(parent),m_deferredFlushTimer(new QTimer(this)) {
    m_deferredFlushTimer->setSingleShot(true);
    connect(m_deferredFlushTimer,&QTimer::timeout,this,[this] {
        if(m_host.flushDeferredProfileSwitchIfIdle) m_host.flushDeferredProfileSwitchIfIdle();
    });
}

void ProfileSwitchCoordinator::setProfileManager(ProfileManager* profileManager) {
    m_profileManager = profileManager;
}

void ProfileSwitchCoordinator::setForegroundMonitor(ForegroundWindowMonitor* monitor) {
    m_foregroundMonitor = monitor;
}

void ProfileSwitchCoordinator::setHostCallbacks(const HostCallbacks& callbacks) {
    m_host = callbacks;
}

void ProfileSwitchCoordinator::scheduleDeferredFlush(int delayMs) {
    if(!m_host.flushDeferredProfileSwitchIfIdle) return;
    delayMs=qMax(1,delayMs);
    if(!m_deferredFlushTimer->isActive() || m_deferredFlushTimer->remainingTime()>delayMs)
        m_deferredFlushTimer->start(delayMs);
}

ForegroundWindowState ProfileSwitchCoordinator::readForeground(const ForegroundWindowState& fallback) {
    if(m_host.foregroundState) return m_host.foregroundState();
#ifdef _WIN32
    if(m_foregroundMonitor) { m_foregroundMonitor->syncFromDesktopForeground();return m_foregroundMonitor->currentState(); }
#endif
    return fallback;
}
void ProfileSwitchCoordinator::observeForeground(const ForegroundWindowState& state) {
    if(state.monotonicSeq!=m_lastForegroundMonotonicSeq) {
        m_lastForegroundMonotonicSeq=state.monotonicSeq;m_foregroundChurnTimer.restart();
    }
}

void ProfileSwitchCoordinator::scheduleDeferredProfileSwitchFlush(int delayMs) {
    scheduleDeferredFlush(delayMs);
}

bool ProfileSwitchCoordinator::foregroundStableForAutoSwitchMs(int requiredMs) const {
    if (!m_foregroundChurnTimer.isValid()) {
        return true;
    }
    return m_foregroundChurnTimer.elapsed() >= requiredMs;
}

void ProfileSwitchCoordinator::onManualProfileSwitchCommitted(const QString& profileId,
                                                              bool isDefaultProfile) {
    m_deferredProfileSwitchId.clear();
    if (isDefaultProfile) {
        m_lastLinkedForegroundProfileId.clear();
        m_recentAutomaticDefaultProfileSwitchTimer.invalidate();
    } else {
        m_lastLinkedForegroundProfileId = profileId;
    }
}

bool ProfileSwitchCoordinator::shouldRestoreLinkedProfileOnPipbongFocus(int windowMs) const {
    return m_recentAutomaticDefaultProfileSwitchTimer.isValid()
           && m_recentAutomaticDefaultProfileSwitchTimer.elapsed() < windowMs;
}

void ProfileSwitchCoordinator::markAutomaticProfileSwitchCommitted() {
    m_lastAutomaticProfileSwitchTimer.start();
}

void ProfileSwitchCoordinator::flushDeferredIfIdle() {
    flushDeferredProfileSwitch();
}

void ProfileSwitchCoordinator::flushDeferredProfileSwitch() {
    if (m_deferredProfileSwitchId.isEmpty() || !m_profileManager) {
        return;
    }
    if(m_host.isWorkspacePinned && m_host.isWorkspacePinned()) { scheduleDeferredFlush(400);return; }
    if (m_host.hasTriggerMonitoringSessions && m_host.hasTriggerMonitoringSessions()) {
        scheduleDeferredFlush(300);
        return;
    }
    if (m_host.isSwitchingProfile && m_host.isSwitchingProfile()) {
        return;
    }
    if (m_host.isPipelineActive && m_host.isPipelineActive()) {
        return;
    }
#ifdef _WIN32
    const auto live=readForeground();
    if((m_host.isPipelineActive && m_host.isPipelineActive()) || m_deferredProfileSwitchId.isEmpty()) return;
    if(live.pipbong) { m_deferredProfileSwitchId.clear();return; }
    if((m_host.transitionModifierHeld ? m_host.transitionModifierHeld() : ForegroundWindowMonitor::isAltTabModifierHeld())
        || (live.shellTransient && !ForegroundWindowMonitor::isDesktopShellHostHwnd(live.rootHwnd))) {
        scheduleDeferredFlush(100);return;
    }
    observeForeground(live);
    m_deferredProfileSwitchId=ProfileForegroundResolver::resolve(*m_profileManager,live).profileId;
    if(!foregroundStableForAutoSwitchMs(320)) { scheduleDeferredFlush(335-static_cast<int>(m_foregroundChurnTimer.elapsed()));return; }
#endif
    const QString profileId = m_deferredProfileSwitchId;
    if (profileId == m_profileManager->activeProfileId()) {
        m_deferredProfileSwitchId.clear();
        return;
    }
    if (ReorderableListWidget::isAnyListDragActive()) {
        scheduleDeferredFlush(50);
        return;
    }
    if(m_host.executeProfileSwitch && m_host.executeProfileSwitch(profileId,true)
        && m_deferredProfileSwitchId==profileId) m_deferredProfileSwitchId.clear();
}

void ProfileSwitchCoordinator::applyFromForegroundState(const ForegroundWindowState& state) {
    if (!m_profileManager) {
        return;
    }
    if(m_host.isWorkspacePinned && m_host.isWorkspacePinned()) {
#ifdef _WIN32
        if(!state.pipbong && !state.shellTransient) {
            const auto resolved=ProfileForegroundResolver::resolve(*m_profileManager,state);
            if(!resolved.profileId.isEmpty() && resolved.profileId!=m_profileManager->activeProfileId()) m_deferredProfileSwitchId=resolved.profileId;
        }
#endif
        if(!m_deferredProfileSwitchId.isEmpty()) scheduleDeferredFlush(400);
        return;
    }
    if (m_host.isSwitchingProfile && m_host.isSwitchingProfile()) {
        return;
    }
    if (m_host.isPipelineActive && m_host.isPipelineActive()) {
        return;
    }
    if (m_host.schedulePruneAbandonedEngines) {
        m_host.schedulePruneAbandonedEngines();
    }

#ifdef _WIN32
    const auto live=readForeground(state);
    observeForeground(live);
    if (live.pipbong) {
        if (m_host.onPipbongForegroundFocus) {
            m_host.onPipbongForegroundFocus();
        }
        return;
    }
    if (!live.rootHwnd) {
        // No HWND — treat as unmatched → default profile via resolve().
    } else if (live.shellTransient
               && !ForegroundWindowMonitor::isDesktopShellHostHwnd(live.rootHwnd)) {
        // Alt+Tab picker, taskbar shell, etc. — do not auto-switch profile.
        return;
    }

    ProfileForegroundResolver::healProfileBindingsFromForeground(*m_profileManager, live);

    const ProfileForegroundResolver::ResolveResult resolved =
        ProfileForegroundResolver::resolve(*m_profileManager, live);

    if (m_host.applyForegroundCaptureHints) {
        m_host.applyForegroundCaptureHints(live.rootHwnd, live.title);
    }

    if (resolved.profileId.isEmpty()
        || resolved.profileId == m_profileManager->activeProfileId()) {
        return;
    }

    if (m_host.hasTriggerMonitoringSessions && m_host.hasTriggerMonitoringSessions()) {
        m_deferredProfileSwitchId = resolved.profileId;
        return;
    }

    if (m_host.transitionModifierHeld ? m_host.transitionModifierHeld() : ForegroundWindowMonitor::isAltTabModifierHeld()) {
        if (!m_profileManager->isDefaultProfile(resolved.profileId)) {
            m_deferredProfileSwitchId = resolved.profileId;
        }
        return;
    }

    if (ReorderableListWidget::isAnyListDragActive()) {
        m_deferredProfileSwitchId = resolved.profileId;
        scheduleDeferredFlush(50);
        return;
    }

    constexpr int kMinAutoSwitchIntervalMs = 200;
    constexpr int kForegroundStabilityMs = 320;
    if (m_lastAutomaticProfileSwitchTimer.isValid()
        && m_lastAutomaticProfileSwitchTimer.elapsed() < kMinAutoSwitchIntervalMs) {
        m_deferredProfileSwitchId = resolved.profileId;
        scheduleDeferredFlush(
            static_cast<int>(kMinAutoSwitchIntervalMs - m_lastAutomaticProfileSwitchTimer.elapsed()));
        return;
    }
    if (!foregroundStableForAutoSwitchMs(kForegroundStabilityMs)) {
        m_deferredProfileSwitchId = resolved.profileId;
        scheduleDeferredFlush(
            static_cast<int>(kForegroundStabilityMs - m_foregroundChurnTimer.elapsed() + 15));
        return;
    }

    if (m_profileManager->isDefaultProfile(resolved.profileId)) {
        m_recentAutomaticDefaultProfileSwitchTimer.start();
    } else {
        m_recentAutomaticDefaultProfileSwitchTimer.invalidate();
        m_lastLinkedForegroundProfileId = resolved.profileId;
    }

    CrashReporter::noteBreadcrumb(QStringLiteral("profile"),
                                  QStringLiteral("switch %1").arg(resolved.profileId));
    if(m_host.executeProfileSwitch && m_host.executeProfileSwitch(resolved.profileId,true)) m_lastAutomaticProfileSwitchTimer.start();
    else m_deferredProfileSwitchId=resolved.profileId;
#else
    Q_UNUSED(state);
#endif
}
