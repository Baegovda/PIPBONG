#pragma once

#include <QString>
#include <QStringList>

#include <vector>

/// Thread-safe diagnostic ring buffers shared by crash/hang reporting.
class DiagnosticHub {
public:
    struct BreadcrumbEntry {
        qint64 epochMs = 0;
        QString category;
        QString message;
    };

    struct WorkerHeartbeatSnapshot {
        unsigned long threadId = 0;
        QString label;
        qint64 lastBeatAgeMs = -1;
    };

    static void noteBreadcrumb(const QString& category, const QString& message);
    static void appendAppLog(const QString& levelTag, const QString& text);
    static void appendAppLogSession(const QString& featureName,
                                    const QString& levelTag,
                                    const QString& text);

    /// Labels the current thread's worker heartbeat (workflow engine worker, etc.).
    static void setWorkerLabel(const QString& label);
    static void clearWorkerLabel();
    static void touchWorkerHeartbeat();

    static QString buildFingerprintSection();
    static QString buildBreadcrumbsText(int maxLines = 200);
    static QString buildAppLogText(int maxLines = 500);
    static QString buildWorkerStatusText();
    static QStringList staleWorkerLabels(qint64 staleThresholdMs);

    static std::vector<BreadcrumbEntry> snapshotBreadcrumbs(int maxLines = 400);
    static std::vector<WorkerHeartbeatSnapshot> snapshotWorkers();

    static bool writeBreadcrumbsFile(const QString& path);
    static bool writeAppLogFile(const QString& path);
    static bool writeWorkerStatusFile(const QString& path);
};
