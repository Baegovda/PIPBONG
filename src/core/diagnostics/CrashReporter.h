#pragma once

#include <QString>

#include <functional>

enum class CrashReportKind {
    Crash,
    Hang,
    QtFatal
};

struct CrashReportSummary {
    QString folderPath;
    QString reportText;
    QString reportFilePath;
    QString dumpFilePath;
    CrashReportKind kind = CrashReportKind::Crash;
};

using CrashContextProvider = std::function<QString()>;

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

    /// Registers a GUI-thread callback that refreshes the cached application context snapshot.
    static void setContextProvider(CrashContextProvider provider);

    /// Records the last notable UI action (button click, etc.) for crash/hang reports.
    static void noteUserAction(const QString& description);

    /// Returns the pending crash from the previous run, if any.
    static bool hasPendingReport();
    static CrashReportSummary pendingReport();

    /// Clears the pending marker after the user dismisses the startup dialog.
    static void dismissPendingReport();

    static QString crashRootDirectory();
    static bool openPathInExplorer(const QString& path);
    static QString latestReportDirectory();

    /// Packages report.txt, recent_log.txt, optional crash.dmp and user_note.txt into a zip.
    static bool exportReportFolderAsZip(const QString& folderPath, const QString& zipPath);

    static CrashReportKind kindFromReportText(const QString& reportText);
    static bool writeUserNote(const QString& folderPath, const QString& note);
};
