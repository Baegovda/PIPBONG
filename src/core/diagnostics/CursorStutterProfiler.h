#pragma once

#include <QString>

#include <cstdint>

/// Cursor jump / stutter diagnostics (v0.8.295+).
/// Sampler always runs; verbose hook/SetCursorPos logging follows program/cursorStutterProfiling
/// or env PIPBONG_CURSOR_STUTTER_PROFILE=1 (default ON).
/// Reports: repo cursor-stutter/latest.md + %LOCALAPPDATA% fallback; written on shutdown and periodic flush.
class CursorStutterProfiler {
public:
    static bool isVerboseLogging();
    static void reloadFromSettings();

    static QString outputDirectory();
    static QString latestReportPath();
    static QStringList allReportPaths();

    /// Starts background GetCursorPos sampler (4 ms). Always call after QApplication exists.
    static void startSampling();
    /// Writes latest.md without stopping the sampler.
    static void flushReport(const QString& reason);
    /// Stops sampler and writes latest.md to all output paths.
    static void stopAndWriteReport(const QString& reason);

    static qint64 monotonicUs();

    static void recordSetCursorPos(const char* caller, int x, int y);
    static void recordClipCursor(const char* caller);
    static void recordMouseHookSnap(int fromX, int fromY, int toX, int toY, qint64 handlerUs);
    static void recordKeyboardHook(int virtualKey, bool keyDown, qint64 handlerUs, bool swallowed);
    static void recordPhysicalKey(int virtualKey);
    static void recordSyntheticKey(int virtualKey);
    /// Hold-mode feature session (feature display name, e.g. LOL profile "Q").
    static void recordHoldFeature(const QString& featureName, const char* phase, qint64 durationUs = -1);
};
