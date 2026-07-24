#pragma once

#include <QString>
#include <QStringList>

/// Opt-in profile switch pipeline diagnostics (manual + automatic).
/// Enabled via program/profileSwitchProfiling or env PIPBONG_PROFILE_SWITCH_PROFILE=1.
/// Report: profile-switch/latest.md (repo mirror when CMakeLists.txt found + AppData backup).
class ProfileSwitchProfiler {
public:
    static bool isEnabled();
    static bool isSwitchInProgress();

    static void beginSwitch(const QString& targetProfileId, const QString& mode);
    static void notePhase(const QString& phase,
                          qint64 durationMs = -1,
                          const QString& detail = QString());
    static void notePingpong();
    static void incrementStartTimerWarningCount();

    static void flushReport(const QString& reason);

    static QString latestReportPath();
    static QStringList allReportPaths();
};
