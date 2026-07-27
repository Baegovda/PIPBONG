#pragma once

#include <QString>

/// Always-on append-only session log for post-mortem analysis when the GUI hangs
/// or the user force-kills the process (crash/hang viewer may not appear).
class LiveSessionLog {
public:
    static void install();
    static void shutdown(const QString& reason = QString());

    static QString primaryLogPath();
    static QStringList allLogPaths();

    /// Thread-safe; batched flush to disk (~400 ms) unless immediate=true.
    static void appendLine(const QString& line, bool immediate = false);

    /// Called from hang watchdog before heavy crash artifact work.
    static void flushHangSnapshot(qint64 silentMs, const QString& contextSnapshot);

private:
    static void flushPendingLines();
};
