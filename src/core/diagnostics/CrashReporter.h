#pragma once

#include <QString>

struct CrashReportSummary {
    QString folderPath;
    QString reportText;
    QString reportFilePath;
    QString dumpFilePath;
};

class CrashReporter {
public:
    /// When argv contains `--crash-report <folder>`, shows the report dialog and returns true.
    /// Call before install() / MainWindow construction.
    static bool runViewerModeIfRequested();

    /// Install Win32/Qt handlers. Call once after QApplication construction.
    static void install();

    /// Heartbeat + background watchdog for GUI-thread hangs ("Not Responding").
    /// Call once on the main thread after install() (not in --crash-report viewer mode).
    static void installGuiHangWatchdog();

    /// Returns the pending crash from the previous run, if any.
    static bool hasPendingReport();
    static CrashReportSummary pendingReport();

    /// Clears the pending marker after the user dismisses the startup dialog.
    static void dismissPendingReport();

    static QString crashRootDirectory();
    static bool openPathInExplorer(const QString& path);
    static QString latestReportDirectory();
};
