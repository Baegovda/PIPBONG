#pragma once

#include "app/ForegroundWindowState.h"

#include <functional>

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class ProfileManager;
class ForegroundWindowMonitor;

class ProfileSwitchCoordinator : public QObject {
    Q_OBJECT

public:
    struct HostCallbacks {
        std::function<bool()> isSwitchingProfile;
        std::function<bool()> isPipelineActive;
        std::function<void()> schedulePruneAbandonedEngines;
        std::function<void(const QString& profileId, bool automatic)> executeProfileSwitch;
        std::function<void(HWND hwnd, const QString& title)> applyForegroundCaptureHints;
        std::function<void()> onPipbongForegroundFocus;
        std::function<void()> flushDeferredProfileSwitchIfIdle;
        /// When true, automatic foreground profile switches are deferred (trigger 감시).
        std::function<bool()> hasTriggerMonitoringSessions;
    };

    explicit ProfileSwitchCoordinator(QObject* parent = nullptr);

    void setProfileManager(ProfileManager* profileManager);
    void setForegroundMonitor(ForegroundWindowMonitor* monitor);
    void setHostCallbacks(const HostCallbacks& callbacks);

    void applyFromForegroundState(const ForegroundWindowState& state);
    void flushDeferredIfIdle();

    /// Full deferred flush (clears deferred when PIPBONG foreground or idle).
    void flushDeferredProfileSwitch();

    QString deferredProfileSwitchId() const { return m_deferredProfileSwitchId; }
    void setDeferredProfileSwitchId(const QString& id) { m_deferredProfileSwitchId = id; }
    void clearDeferredProfileSwitchId() { m_deferredProfileSwitchId.clear(); }

    QString lastLinkedForegroundProfileId() const { return m_lastLinkedForegroundProfileId; }
    void setLastLinkedForegroundProfileId(const QString& id) { m_lastLinkedForegroundProfileId = id; }
    void clearLastLinkedForegroundProfileId() { m_lastLinkedForegroundProfileId.clear(); }

    void invalidateRecentAutomaticDefaultSwitchTimer() {
        m_recentAutomaticDefaultProfileSwitchTimer.invalidate();
    }

    void onManualProfileSwitchCommitted(const QString& profileId, bool isDefaultProfile);

    bool shouldRestoreLinkedProfileOnPipbongFocus(int windowMs) const;

    void markAutomaticProfileSwitchCommitted();

    /// Schedule a deferred auto-switch flush (e.g. after profile pipeline completes).
    void scheduleDeferredProfileSwitchFlush(int delayMs);

private:
    void scheduleDeferredFlush(int delayMs);
    bool foregroundStableForAutoSwitchMs(int requiredMs) const;

    ProfileManager* m_profileManager = nullptr;
    ForegroundWindowMonitor* m_foregroundMonitor = nullptr;
    HostCallbacks m_host;

    QString m_deferredProfileSwitchId;
    QElapsedTimer m_lastAutomaticProfileSwitchTimer;
    QElapsedTimer m_recentAutomaticDefaultProfileSwitchTimer;
    QString m_lastLinkedForegroundProfileId;
    quint64 m_lastForegroundMonotonicSeq = 0;
    QElapsedTimer m_foregroundChurnTimer;
};
