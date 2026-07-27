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
    : QObject(parent) {}

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
    if (!m_host.flushDeferredProfileSwitchIfIdle) {
        return;
    }
    QTimer::singleShot(delayMs, this, [this]() { m_host.flushDeferredProfileSwitchIfIdle(); });
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
    if (m_host.isSwitchingProfile && m_host.isSwitchingProfile()) {
        return;
    }
    if (m_host.isPipelineActive && m_host.isPipelineActive()) {
        return;
    }
#ifdef _WIN32
    if (m_foregroundMonitor) {
        m_foregroundMonitor->syncFromDesktopForeground();
        if (m_foregroundMonitor->isPipbongForeground()) {
            m_deferredProfileSwitchId.clear();
            return;
        }
    }
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
    m_deferredProfileSwitchId.clear();
    if (m_host.executeProfileSwitch) {
        m_host.executeProfileSwitch(profileId, true);
    }
}

void ProfileSwitchCoordinator::applyFromForegroundState(const ForegroundWindowState& state) {
    if (!m_profileManager) {
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
    if (m_foregroundMonitor) {
        m_foregroundMonitor->syncFromDesktopForeground();
    }
    const ForegroundWindowState& live =
        m_foregroundMonitor ? m_foregroundMonitor->currentState() : state;
    if (live.pipbong) {
        if (m_host.onPipbongForegroundFocus) {
            m_host.onPipbongForegroundFocus();
        }
        return;
    }
    if (!live.rootHwnd || live.shellTransient) {
        return;
    }

    const ProfileForegroundResolver::ResolveResult resolved =
        ProfileForegroundResolver::resolve(*m_profileManager, live);

    if (m_host.applyForegroundCaptureHints) {
        m_host.applyForegroundCaptureHints(live.rootHwnd, live.title);
    }

    if (resolved.profileId.isEmpty()
        || resolved.profileId == m_profileManager->activeProfileId()) {
        return;
    }

    if (ForegroundWindowMonitor::isAltTabModifierHeld()) {
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

    constexpr int kMinAutoSwitchIntervalMs = 80;
    if (m_lastAutomaticProfileSwitchTimer.isValid()
        && m_lastAutomaticProfileSwitchTimer.elapsed() < kMinAutoSwitchIntervalMs) {
        m_deferredProfileSwitchId = resolved.profileId;
        scheduleDeferredFlush(
            static_cast<int>(kMinAutoSwitchIntervalMs - m_lastAutomaticProfileSwitchTimer.elapsed()));
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
    if (m_host.executeProfileSwitch) {
        m_host.executeProfileSwitch(resolved.profileId, true);
    }
    m_lastAutomaticProfileSwitchTimer.start();
#else
    Q_UNUSED(state);
#endif
}
