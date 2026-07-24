#pragma once

#include <QString>

/// Opt-in profile auto-switch pipeline diagnostics.
/// Enabled via program/profileSwitchProfiling or env PIPBONG_PROFILE_SWITCH_PROFILE=1.
/// Report: %LOCALAPPDATA%/PIPBONG/PIPBONG/profile-switch/latest.md
class ProfileSwitchProfiler {
public:
    static bool isEnabled();

    static void beginSwitch(const QString& targetProfileId);
    static void notePhase(const QString& phase,
                          qint64 durationMs = -1,
                          const QString& detail = QString());
    static void notePingpong();
    static void incrementStartTimerWarningCount();

    static void flushReport(const QString& reason);
};
