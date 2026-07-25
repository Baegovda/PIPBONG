#pragma once

#include <QString>

#include <cstdint>

/// Background app stutter / UI stall diagnostics (single unified profiler).
/// Opt-in via program/appStutterProfiling or env PIPBONG_APP_STUTTER_PROFILE=1 (default OFF).
/// Reports: repo app-stutter/latest.md + %LOCALAPPDATA% fallback; also flushed on crash.
class AppStutterProfiler {
public:
    static bool isEnabled();
    static void reloadFromSettings();

    static QString outputDirectory();
    static QString latestReportPath();
    static QStringList allReportPaths();

    static void flushReport(const QString& reason);
    static void stopAndWriteReport(const QString& reason);

    /// Called from CrashReporter before writing crash artifacts (always writes report).
    static void flushOnCrash();

    /// Called from a 50 ms GUI timer while profiling is enabled.
    static void noteGuiPulse();

    /// Updated when feature run sessions change (MainWindow).
    static void setActiveFeatureSessionCount(int count);
    static void setPipbongFeatureBurstActive(bool active);
};

/// RAII scope for named main-thread or worker operations (profile switch, trigger, run, …).
class AppStutterOperationScope {
public:
    AppStutterOperationScope(const char* category, const QString& label, const char* threadTag = "main");
    ~AppStutterOperationScope();

    AppStutterOperationScope(const AppStutterOperationScope&) = delete;
    AppStutterOperationScope& operator=(const AppStutterOperationScope&) = delete;

    void setDetail(const QString& extra);

private:
    const char* m_category = nullptr;
    QString m_label;
    const char* m_threadTag = "main";
    qint64 m_startUs = 0;
    bool m_active = false;
};
