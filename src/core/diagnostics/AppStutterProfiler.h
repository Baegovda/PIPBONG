#pragma once

#include <QString>

#include <cstdint>

/// Background app stutter / UI stall diagnostics (single unified profiler).
/// Opt-in via program/appStutterProfiling or env PIPBONG_APP_STUTTER_PROFILE=1 (default OFF).
/// Reports: repo app-stutter/latest.md + %LOCALAPPDATA% fallback; written on app shutdown.
class AppStutterProfiler {
public:
    static bool isEnabled();
    static void reloadFromSettings();

    static QString outputDirectory();
    static QString latestReportPath();
    static QStringList allReportPaths();

    static void flushReport(const QString& reason);
    static void stopAndWriteReport(const QString& reason);

    /// Called from a 50 ms GUI timer while profiling is enabled.
    static void noteGuiPulse();

    /// Updated when feature run sessions change (MainWindow).
    static void setActiveFeatureSessionCount(int count);
    static void setPipbongFeatureBurstActive(bool active);
};
