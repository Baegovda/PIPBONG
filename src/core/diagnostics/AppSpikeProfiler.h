#pragma once

#include <QString>

#include <cstdint>

/// Background app spike / UI stall diagnostics (replaces CursorStutterProfiler).
/// Opt-in via program/appSpikeProfiling or env PIPBONG_APP_SPIKE_PROFILE=1 (default OFF).
/// Reports: repo app-spike/latest.md + %LOCALAPPDATA% fallback; written on app shutdown.
class AppSpikeProfiler {
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
