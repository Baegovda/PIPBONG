#include "core/diagnostics/LiveSessionLog.h"

#include "core/diagnostics/DiagnosticHub.h"
#include "PipbongVersion.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

#include <deque>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int kFlushIntervalMs = 400;
constexpr int kMaxPendingLines = 8000;
constexpr qint64 kMaxLogFileBytes = 6 * 1024 * 1024;

std::mutex g_mutex;
std::deque<QString> g_pending;
bool g_installed = false;
QString g_appDataLogPath;
QString g_repoLogPath;

QString findRepoRootDirectory() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"PIPBONG_REPO_ROOT", buffer, MAX_PATH) > 0) {
        return QDir(QString::fromWCharArray(buffer)).absolutePath();
    }
#endif
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth) {
        const QString cmakePath = dir.absoluteFilePath(QStringLiteral("CMakeLists.txt"));
        if (QFile::exists(cmakePath)) {
            QFile cmakeFile(cmakePath);
            if (cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QByteArray head = cmakeFile.read(256);
                if (head.contains("PIPBONG")) {
                    return dir.absolutePath();
                }
            }
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

QString appDataLiveSessionDirectory() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        return QString();
    }
    const QString dirPath = QDir(base).filePath(QStringLiteral("PIPBONG/PIPBONG/live-session"));
    QDir().mkpath(dirPath);
    return dirPath;
}

bool appendUtf8LineToFile(const QString& path, const QString& line) {
    if (path.isEmpty()) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    const qint64 sizeBefore = file.size();
    if (sizeBefore > kMaxLogFileBytes) {
        file.close();
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QByteArray all = in.readAll();
        in.close();
        const int keep = static_cast<int>(kMaxLogFileBytes / 2);
        const QByteArray tail = all.size() > keep ? all.right(keep) : all;
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return false;
        }
        file.write(tail);
        file.write("\n# --- log truncated (size cap) ---\n");
    }
    const QByteArray bytes = line.toUtf8();
    if (!bytes.endsWith('\n')) {
        file.write(bytes);
        file.write("\n");
    } else {
        file.write(bytes);
    }
    file.flush();
    return true;
}

void writeToAllSinks(const QString& line) {
    if (!g_appDataLogPath.isEmpty()) {
        appendUtf8LineToFile(g_appDataLogPath, line);
    }
    if (!g_repoLogPath.isEmpty() && g_repoLogPath != g_appDataLogPath) {
        appendUtf8LineToFile(g_repoLogPath, line);
    }
}

QString sessionHeaderText() {
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QString exePath = QCoreApplication::applicationFilePath();
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    const DWORD pid = 0;
#endif
    return QStringLiteral(
               "# PIPBONG live session log (always-on; for 응답없음 / force-kill analysis)\n"
               "# session_start: %1\n"
               "# version: %2  git: %3  pid: %4\n"
               "# exe: %5\n"
               "# primary: %6\n"
               "---\n")
               .arg(timestamp,
                    QString::fromUtf8(PIPBONG_VERSION),
                    QString::fromUtf8(PIPBONG_GIT_SHA),
                    QString::number(pid),
                    exePath,
                    g_appDataLogPath);
}

void openNewSessionFiles() {
    g_appDataLogPath = QDir(appDataLiveSessionDirectory()).filePath(QStringLiteral("latest.log"));
    const QString repo = findRepoRootDirectory();
    if (!repo.isEmpty()) {
        const QString dir = QDir(repo).filePath(QStringLiteral("live-session"));
        QDir().mkpath(dir);
        g_repoLogPath = QDir(dir).filePath(QStringLiteral("latest.log"));
    } else {
        g_repoLogPath.clear();
    }

    if (!g_appDataLogPath.isEmpty()) {
        QFile::remove(g_appDataLogPath);
    }
    if (!g_repoLogPath.isEmpty() && g_repoLogPath != g_appDataLogPath) {
        QFile::remove(g_repoLogPath);
    }

    const QString header = sessionHeaderText();
    writeToAllSinks(header);
}

} // namespace

void LiveSessionLog::install() {
    if (g_installed) {
        return;
    }
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        return;
    }

    openNewSessionFiles();
    g_installed = true;

    auto* flushTimer = new QTimer(app);
    flushTimer->setInterval(kFlushIntervalMs);
    QObject::connect(flushTimer, &QTimer::timeout, app, []() { flushPendingLines(); });
    flushTimer->start();

    QObject::connect(app, &QCoreApplication::aboutToQuit, app, []() {
        shutdown(QStringLiteral("aboutToQuit"));
    });
}

void LiveSessionLog::shutdown(const QString& reason) {
    if (!g_installed) {
        return;
    }
    const QString tag = reason.trimmed().isEmpty() ? QStringLiteral("shutdown") : reason.trimmed();
    appendLine(QStringLiteral("# session_end: %1 (%2)")
                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                        tag),
               true);
    flushPendingLines();
}

QString LiveSessionLog::primaryLogPath() {
    return g_appDataLogPath;
}

QStringList LiveSessionLog::allLogPaths() {
    QStringList paths;
    if (!g_appDataLogPath.isEmpty()) {
        paths << g_appDataLogPath;
    }
    if (!g_repoLogPath.isEmpty() && g_repoLogPath != g_appDataLogPath) {
        paths << g_repoLogPath;
    }
    return paths;
}

void LiveSessionLog::appendLine(const QString& line, bool immediate) {
    if (line.isEmpty() || !g_installed) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pending.push_back(line);
        while (g_pending.size() > kMaxPendingLines) {
            g_pending.pop_front();
        }
    }
    if (immediate) {
        flushPendingLines();
    }
}

void LiveSessionLog::flushPendingLines() {
    std::deque<QString> batch;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_pending.empty()) {
            return;
        }
        batch.swap(g_pending);
    }
    for (const QString& line : batch) {
        writeToAllSinks(line);
    }
}

void LiveSessionLog::flushHangSnapshot(qint64 silentMs, const QString& contextSnapshot) {
    if (!g_installed) {
        return;
    }

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    appendLine(QStringLiteral("!!! GUI_HANG_DETECTED %1 silent_ms=%2").arg(timestamp).arg(silentMs),
               true);

    const QString breadcrumbs = DiagnosticHub::buildBreadcrumbsText(120);
    if (!breadcrumbs.trimmed().isEmpty()) {
        appendLine(QStringLiteral("--- breadcrumbs (tail) ---"), true);
        const QStringList lines = breadcrumbs.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            appendLine(line, true);
        }
    }

    const QString appLog = DiagnosticHub::buildAppLogText(200);
    if (!appLog.trimmed().isEmpty()) {
        appendLine(QStringLiteral("--- app_log (tail) ---"), true);
        const QStringList lines = appLog.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            appendLine(line, true);
        }
    }

    const QString workers = DiagnosticHub::buildWorkerStatusText();
    if (!workers.trimmed().isEmpty()) {
        appendLine(QStringLiteral("--- worker_status ---"), true);
        appendLine(workers, true);
    }

    if (!contextSnapshot.trimmed().isEmpty()) {
        appendLine(QStringLiteral("--- application_context ---"), true);
        appendLine(contextSnapshot, true);
    }

    appendLine(QStringLiteral("--- end GUI_HANG_DETECTED ---"), true);
    flushPendingLines();
}
