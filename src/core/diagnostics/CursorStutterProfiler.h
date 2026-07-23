#pragma once

#include <QString>

#include <cstdint>

/// Cursor jump / stutter diagnostics — replaces WorkflowRunProfiler (v0.8.295+).
/// Enable: program/cursorStutterProfiling or env PIPBONG_CURSOR_STUTTER_PROFILE=1.
/// Output: <repo-root>/cursor-stutter/latest.md on app exit.
class CursorStutterProfiler {
public:
    static bool isEnabled();
    static void reloadFromSettings();

    static QString outputDirectory();
    static QString latestReportPath();

    /// Starts background GetCursorPos sampler (4 ms). Call once after QApplication exists.
    static void startSampling();
    /// Stops sampler and writes latest.md.
    static void stopAndWriteReport(const QString& reason);

    static qint64 monotonicUs();

    static void recordSetCursorPos(const char* caller, int x, int y);
    static void recordClipCursor(const char* caller);
    static void recordMouseHookSnap(int fromX, int fromY, int toX, int toY, qint64 handlerUs);
    static void recordKeyboardHook(int virtualKey, bool keyDown, qint64 handlerUs, bool swallowed);
    static void recordPhysicalKey(int virtualKey);
    static void recordSyntheticKey(int virtualKey);
};
