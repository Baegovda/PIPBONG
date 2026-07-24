#pragma once

#include <QString>
#include <QStringList>

/// Opt-in multi-Hold hotkey burst diagnostics (Q/W/E/R simultaneous press/release).
/// Enabled via program/multiHoldProfiling or env PIPBONG_MULTI_HOLD_PROFILE=1.
/// Report: multi-hold/latest.md (repo mirror when CMakeLists.txt found + AppData backup).
class MultiHoldProfiler {
public:
    static bool isEnabled();

    static void notePhase(const QString& phase,
                          qint64 durationMs = -1,
                          const QString& detail = QString());
    static void flushReport(const QString& reason);

    static QString latestReportPath();
    static QStringList allReportPaths();
};
