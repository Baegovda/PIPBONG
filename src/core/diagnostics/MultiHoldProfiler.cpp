#include "core/diagnostics/MultiHoldProfiler.h"

#include "app/ProgramSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int kMaxPhaseRows = 256;

struct PhaseRow {
    qint64 relMs = 0;
    QString phase;
    qint64 durationMs = -1;
    QString detail;
};

std::mutex g_mutex;
qint64 g_sessionStartMs = 0;
std::vector<PhaseRow> g_phases;

qint64 steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

qint64 relMsLocked() {
    if (g_sessionStartMs <= 0) {
        return 0;
    }
    return steadyMs() - g_sessionStartMs;
}

bool envEnabled() {
    return qEnvironmentVariableIsSet("PIPBONG_MULTI_HOLD_PROFILE")
           && qEnvironmentVariable("PIPBONG_MULTI_HOLD_PROFILE") == QLatin1String("1");
}

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

QString appDataOutputDirectory() {
    const QString local = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(local).filePath(QStringLiteral("multi-hold"));
}

QString repoOutputDirectory() {
    const QString repo = findRepoRootDirectory();
    if (repo.isEmpty()) {
        return QString();
    }
    return QDir(repo).filePath(QStringLiteral("multi-hold"));
}

void trimPhasesLocked() {
    if (static_cast<int>(g_phases.size()) <= kMaxPhaseRows) {
        return;
    }
    const int drop = static_cast<int>(g_phases.size()) - kMaxPhaseRows;
    g_phases.erase(g_phases.begin(), g_phases.begin() + drop);
}

QString phaseLabelKo(const QString& phase) {
    if (phase == QStringLiteral("hold_burst_prep_ms")) {
        return QStringLiteral("홀드 버스트 foreground 준비");
    }
    if (phase == QStringLiteral("hold_burst_prep_skipped")) {
        return QStringLiteral("foreground 준비 생략(캐시)");
    }
    if (phase == QStringLiteral("hold_start_ms")) {
        return QStringLiteral("홀드 시작 처리");
    }
    if (phase == QStringLiteral("hold_start_ui_ms")) {
        return QStringLiteral("홀드 시작 UI 묶음");
    }
    if (phase == QStringLiteral("hold_end_cleanup_ms")) {
        return QStringLiteral("홀드 종료 정리 묶음");
    }
    if (phase == QStringLiteral("apply_run_ui_ms")) {
        return QStringLiteral("applyRunUiState");
    }
    return phase;
}

QString buildReportBody(const QString& reason) {
    std::lock_guard<std::mutex> lock(g_mutex);

    QStringList lines;
    lines << QStringLiteral("---");
    lines << QStringLiteral("format: pipbong-multi-hold");
    lines << QStringLiteral("format_version: 1");
    lines << QStringLiteral("end_reason: %1").arg(reason);
    lines << QStringLiteral("session_end: %1")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    lines << QStringLiteral("profiling_enabled: yes");
    lines << QStringLiteral("phase_rows: %1").arg(g_phases.size());

    const QStringList paths = MultiHoldProfiler::allReportPaths();
    if (!paths.isEmpty()) {
        lines << QStringLiteral("report_paths: %1").arg(paths.join(QLatin1Char(';')));
    }
    lines << QStringLiteral("---");
    lines << QString();
    lines << QStringLiteral("# Multi-Hold burst report");
    lines << QString();
    lines << QStringLiteral("## Auto diagnosis");
    lines << QString();

    qint64 maxMs = 0;
    QString slowestPhase;
    for (const PhaseRow& row : g_phases) {
        if (row.durationMs > maxMs) {
            maxMs = row.durationMs;
            slowestPhase = row.phase;
        }
    }
    if (maxMs > 0) {
        lines << QStringLiteral("- **가장 느린 단계**: %1 **%2 ms**")
                     .arg(phaseLabelKo(slowestPhase))
                     .arg(maxMs);
    } else {
        lines << QStringLiteral("- 기록된 단계 타이밍 없음 (이벤트만 수집됨)");
    }
    lines << QStringLiteral("- **AI**: `hold_burst_prep_ms` / `hold_start_ui_ms` / `hold_end_cleanup_ms` / "
                            "`apply_run_ui_ms` 로 병목 확인. 워크플로 구성을 사용자에게 다시 묻지 마세요.");
    lines << QString();
    lines << QStringLiteral("## Phase log (tsv)");
    lines << QString();
    lines << QStringLiteral("```tsv");
    lines << QStringLiteral("rel_ms\tphase\tduration_ms\tdetail");
    for (const PhaseRow& row : g_phases) {
        lines << QStringLiteral("%1\t%2\t%3\t%4")
                     .arg(row.relMs)
                     .arg(row.phase)
                     .arg(row.durationMs)
                     .arg(row.detail);
    }
    lines << QStringLiteral("```");
    lines << QString();

    g_phases.clear();
    g_sessionStartMs = 0;
    return lines.join(QLatin1Char('\n'));
}

void writeReport(const QString& body) {
    const QStringList dirs = {repoOutputDirectory(), appDataOutputDirectory()};
    for (const QString& dirPath : dirs) {
        if (dirPath.isEmpty()) {
            continue;
        }
        QDir().mkpath(dirPath);
        QFile file(QDir(dirPath).filePath(QStringLiteral("latest.md")));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            file.write(body.toUtf8());
        }
    }
}

} // namespace

bool MultiHoldProfiler::isEnabled() {
    return envEnabled() || ProgramSettings::multiHoldProfiling();
}

void MultiHoldProfiler::notePhase(const QString& phase, qint64 durationMs, const QString& detail) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_sessionStartMs <= 0) {
        g_sessionStartMs = steadyMs();
    }
    PhaseRow row;
    row.relMs = relMsLocked();
    row.phase = phase;
    row.durationMs = durationMs;
    row.detail = detail;
    g_phases.push_back(row);
    trimPhasesLocked();
}

void MultiHoldProfiler::flushReport(const QString& reason) {
    if (!isEnabled()) {
        return;
    }
    writeReport(buildReportBody(reason));
}

QString MultiHoldProfiler::latestReportPath() {
    const QString repo = repoOutputDirectory();
    if (!repo.isEmpty()) {
        const QString path = QDir(repo).filePath(QStringLiteral("latest.md"));
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
}

QStringList MultiHoldProfiler::allReportPaths() {
    QStringList paths;
    const QString repo = repoOutputDirectory();
    if (!repo.isEmpty()) {
        paths << QDir(repo).filePath(QStringLiteral("latest.md"));
    }
    paths << QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
    return paths;
}
