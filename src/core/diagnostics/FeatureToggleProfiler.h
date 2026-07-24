#pragma once

#include <QString>
#include <QStringList>

/// Opt-in feature enable/disable (사용 ON/OFF) toggle diagnostics.
/// Enabled via program/featureToggleProfiling or env PIPBONG_FEATURE_TOGGLE_PROFILE=1.
/// Report: feature-toggle/latest.md (repo mirror when CMakeLists.txt found + AppData backup).
class FeatureToggleProfiler {
public:
    static bool isEnabled();
    static bool isToggleInProgress();

    static void beginToggle(const QString& featureId, bool enabled);
    static void notePhase(const QString& phase,
                          qint64 durationMs = -1,
                          const QString& detail = QString());
    static void flushReport(const QString& reason);

    static QString latestReportPath();
    static QStringList allReportPaths();
};
