#include "core/diagnostics/DiagnosticHub.h"

#include "PipbongVersion.h"

#include <opencv2/core/version.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QThread>

#include <deque>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <QStringConverter>

namespace {

constexpr int kMaxBreadcrumbLines = 400;
constexpr int kMaxAppLogLines = 1200;

std::mutex g_breadcrumbMutex;
std::deque<DiagnosticHub::BreadcrumbEntry> g_breadcrumbs;

std::mutex g_appLogMutex;
std::deque<QString> g_appLogLines;

struct WorkerHeartbeatEntry {
    QString label;
    qint64 lastBeatMs = 0;
};

std::mutex g_workerMutex;
std::unordered_map<unsigned long, WorkerHeartbeatEntry> g_workerHeartbeats;

QString formatAppLogLine(const QString& levelTag, const QString& text) {
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString tag = levelTag.trimmed().isEmpty() ? QStringLiteral("INFO") : levelTag.trimmed();
    return QStringLiteral("[%1] %2 %3").arg(timestamp, tag, text);
}

QString formatAppLogSessionLine(const QString& featureName,
                                const QString& levelTag,
                                const QString& text) {
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString tag = levelTag.trimmed().isEmpty() ? QStringLiteral("INFO") : levelTag.trimmed();
    const QString featureTag =
        featureName.isEmpty() ? QString() : QStringLiteral("[%1] ").arg(featureName);
    return QStringLiteral("[%1] %2 %3%4")
        .arg(timestamp, tag, featureTag, text);
}

unsigned long currentThreadId() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return static_cast<unsigned long>(reinterpret_cast<quintptr>(QThread::currentThreadId()));
#endif
}

bool writeUtf8File(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << text;
    return true;
}

} // namespace

void DiagnosticHub::noteBreadcrumb(const QString& category, const QString& message) {
    const QString trimmedCategory = category.trimmed();
    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.isEmpty()) {
        return;
    }

    DiagnosticHub::BreadcrumbEntry entry;
    entry.epochMs = QDateTime::currentMSecsSinceEpoch();
    entry.category = trimmedCategory.isEmpty() ? QStringLiteral("app") : trimmedCategory;
    entry.message = trimmedMessage;

    std::lock_guard<std::mutex> lock(g_breadcrumbMutex);
    g_breadcrumbs.push_back(std::move(entry));
    while (g_breadcrumbs.size() > kMaxBreadcrumbLines) {
        g_breadcrumbs.pop_front();
    }
}

void DiagnosticHub::appendAppLog(const QString& levelTag, const QString& text) {
    if (text.trimmed().isEmpty()) {
        return;
    }
    const QString line = formatAppLogLine(levelTag, text.trimmed());
    std::lock_guard<std::mutex> lock(g_appLogMutex);
    g_appLogLines.push_back(line);
    while (g_appLogLines.size() > kMaxAppLogLines) {
        g_appLogLines.pop_front();
    }
}

void DiagnosticHub::appendAppLogSession(const QString& featureName,
                                        const QString& levelTag,
                                        const QString& text) {
    if (text.trimmed().isEmpty()) {
        return;
    }
    const QString line = formatAppLogSessionLine(featureName.trimmed(), levelTag, text.trimmed());
    std::lock_guard<std::mutex> lock(g_appLogMutex);
    g_appLogLines.push_back(line);
    while (g_appLogLines.size() > kMaxAppLogLines) {
        g_appLogLines.pop_front();
    }
}

void DiagnosticHub::setWorkerLabel(const QString& label) {
    const unsigned long threadId = currentThreadId();
    std::lock_guard<std::mutex> lock(g_workerMutex);
    WorkerHeartbeatEntry& entry = g_workerHeartbeats[threadId];
    entry.label = label.trimmed();
    entry.lastBeatMs = QDateTime::currentMSecsSinceEpoch();
}

void DiagnosticHub::clearWorkerLabel() {
    const unsigned long threadId = currentThreadId();
    std::lock_guard<std::mutex> lock(g_workerMutex);
    g_workerHeartbeats.erase(threadId);
}

void DiagnosticHub::touchWorkerHeartbeat() {
    const unsigned long threadId = currentThreadId();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::lock_guard<std::mutex> lock(g_workerMutex);
    auto it = g_workerHeartbeats.find(threadId);
    if (it == g_workerHeartbeats.end()) {
        WorkerHeartbeatEntry entry;
        entry.label = QStringLiteral("worker@0x%1").arg(threadId, 0, 16);
        entry.lastBeatMs = now;
        g_workerHeartbeats.emplace(threadId, std::move(entry));
        return;
    }
    it->second.lastBeatMs = now;
}

QString DiagnosticHub::buildFingerprintSection() {
    QStringList lines;
    lines.append(QStringLiteral("version: %1").arg(QStringLiteral(PIPBONG_VERSION)));
    lines.append(QStringLiteral("gitSha: %1").arg(QStringLiteral(PIPBONG_GIT_SHA)));
    lines.append(QStringLiteral("build: %1 %2").arg(QStringLiteral(__DATE__), QStringLiteral(__TIME__)));
    if (QCoreApplication::instance()) {
        lines.append(QStringLiteral("qt: %1").arg(QString::fromLatin1(qVersion())));
    }
    lines.append(QStringLiteral("opencv: %1").arg(QString::fromLatin1(CV_VERSION)));
#ifdef _WIN32
    lines.append(QStringLiteral("processId: %1").arg(GetCurrentProcessId()));
    lines.append(QStringLiteral("mainThreadId: 0x%1").arg(GetCurrentThreadId(), 0, 16));
#endif
    return lines.join(QLatin1Char('\n'));
}

std::vector<DiagnosticHub::BreadcrumbEntry> DiagnosticHub::snapshotBreadcrumbs(int maxLines) {
    std::vector<BreadcrumbEntry> snapshot;
    std::lock_guard<std::mutex> lock(g_breadcrumbMutex);
    const int count = static_cast<int>(g_breadcrumbs.size());
    const int start = std::max(0, count - std::max(1, maxLines));
    snapshot.reserve(static_cast<size_t>(count - start));
    for (int index = start; index < count; ++index) {
        const DiagnosticHub::BreadcrumbEntry& entry = g_breadcrumbs.at(static_cast<size_t>(index));
        DiagnosticHub::BreadcrumbEntry copy;
        copy.epochMs = entry.epochMs;
        copy.category = entry.category;
        copy.message = entry.message;
        snapshot.push_back(std::move(copy));
    }
    return snapshot;
}

std::vector<DiagnosticHub::WorkerHeartbeatSnapshot> DiagnosticHub::snapshotWorkers() {
    std::vector<WorkerHeartbeatSnapshot> snapshot;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::lock_guard<std::mutex> lock(g_workerMutex);
    snapshot.reserve(g_workerHeartbeats.size());
    for (const auto& pair : g_workerHeartbeats) {
        WorkerHeartbeatSnapshot entry;
        entry.threadId = pair.first;
        entry.label = pair.second.label;
        entry.lastBeatAgeMs =
            pair.second.lastBeatMs > 0 ? now - pair.second.lastBeatMs : -1;
        snapshot.push_back(std::move(entry));
    }
    return snapshot;
}

QString DiagnosticHub::buildBreadcrumbsText(int maxLines) {
    QStringList lines;
    lines.append(QStringLiteral("PIPBONG diagnostic breadcrumbs (newest last)"));
    lines.append(QString());

    std::lock_guard<std::mutex> lock(g_breadcrumbMutex);
    const int count = static_cast<int>(g_breadcrumbs.size());
    const int start = std::max(0, count - std::max(1, maxLines));
    for (int index = start; index < count; ++index) {
        const DiagnosticHub::BreadcrumbEntry& entry = g_breadcrumbs.at(static_cast<size_t>(index));
        const QString timestamp =
            QDateTime::fromMSecsSinceEpoch(entry.epochMs).toString(Qt::ISODateWithMs);
        lines.append(QStringLiteral("%1 [%2] %3").arg(timestamp, entry.category, entry.message));
    }
    if (count == 0) {
        lines.append(QStringLiteral("(none)"));
    }
    return lines.join(QLatin1Char('\n'));
}

QString DiagnosticHub::buildAppLogText(int maxLines) {
    QStringList lines;
    lines.append(QStringLiteral("PIPBONG application log mirror (newest last)"));
    lines.append(QString());

    std::lock_guard<std::mutex> lock(g_appLogMutex);
    const int count = static_cast<int>(g_appLogLines.size());
    const int start = std::max(0, count - std::max(1, maxLines));
    for (int index = start; index < count; ++index) {
        lines.append(g_appLogLines.at(static_cast<size_t>(index)));
    }
    if (count == 0) {
        lines.append(QStringLiteral("(none)"));
    }
    return lines.join(QLatin1Char('\n'));
}

QString DiagnosticHub::buildWorkerStatusText() {
    QStringList lines;
    lines.append(QStringLiteral("PIPBONG worker heartbeats"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    std::lock_guard<std::mutex> lock(g_workerMutex);
    if (g_workerHeartbeats.empty()) {
        lines.append(QStringLiteral("(no registered workers)"));
        return lines.join(QLatin1Char('\n'));
    }

    for (const auto& pair : g_workerHeartbeats) {
        const qint64 ageMs = pair.second.lastBeatMs > 0 ? now - pair.second.lastBeatMs : -1;
        lines.append(QStringLiteral("  thread=0x%1 label=\"%2\" lastBeat=%3 ms ago")
                         .arg(pair.first, 0, 16)
                         .arg(pair.second.label)
                         .arg(ageMs));
    }
    return lines.join(QLatin1Char('\n'));
}

QStringList DiagnosticHub::staleWorkerLabels(qint64 staleThresholdMs) {
    QStringList stale;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::lock_guard<std::mutex> lock(g_workerMutex);
    for (const auto& pair : g_workerHeartbeats) {
        if (pair.second.lastBeatMs <= 0) {
            continue;
        }
        const qint64 ageMs = now - pair.second.lastBeatMs;
        if (ageMs >= staleThresholdMs) {
            stale.append(QStringLiteral("%1 (%2 ms silent)")
                             .arg(pair.second.label.isEmpty() ? QStringLiteral("(unnamed)")
                                                              : pair.second.label)
                             .arg(ageMs));
        }
    }
    return stale;
}

bool DiagnosticHub::writeBreadcrumbsFile(const QString& path) {
    return writeUtf8File(path, buildBreadcrumbsText());
}

bool DiagnosticHub::writeAppLogFile(const QString& path) {
    return writeUtf8File(path, buildAppLogText());
}

bool DiagnosticHub::writeWorkerStatusFile(const QString& path) {
    return writeUtf8File(path, buildWorkerStatusText());
}
