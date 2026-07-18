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
    /// Install Win32/Qt handlers. Call once after QApplication construction.
    static void install();

    /// Returns the pending crash from the previous run, if any.
    static bool hasPendingReport();
    static CrashReportSummary pendingReport();

    /// Clears the pending marker after the user dismisses the startup dialog.
    static void dismissPendingReport();

    static QString crashRootDirectory();
    static bool openPathInExplorer(const QString& path);
    static QString latestReportDirectory();
};
