#include "core/diagnostics/FeatureToggleProfiler.h"

#include "app/ProgramSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>
#include <atomic>
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

constexpr int kMaxPhaseRows = 128;

struct PhaseRow {
    qint64 relMs = 0;
    QString phase;
    qint64 durationMs = -1;
    QString detail;
};

std::mutex g_mutex;
std::atomic<bool> g_toggleActive{false};
QString g_featureId;
bool g_enabled = true;
qint64 g_toggleStartMs = 0;
std::vector<PhaseRow> g_phases;

qint64 steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

qint64 relMsLocked() {
    if (g_toggleStartMs <= 0) {
        return 0;
    }
    return steadyMs() - g_toggleStartMs;
}

bool envEnabled() {
    return qEnvironmentVariableIsSet("PIPBONG_FEATURE_TOGGLE_PROFILE")
           && qEnvironmentVariable("PIPBONG_FEATURE_TOGGLE_PROFILE") == QLatin1String("1");
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
    return QDir(local).filePath(QStringLiteral("feature-toggle"));
}

QString repoOutputDirectory() {
    const QString repo = findRepoRootDirectory();
    if (repo.isEmpty()) {
        return QString();
    }
    return QDir(repo).filePath(QStringLiteral("feature-toggle"));
}

void trimPhasesLocked() {
    if (static_cast<int>(g_phases.size()) <= kMaxPhaseRows) {
        return;
    }
    const int drop = static_cast<int>(g_phases.size()) - kMaxPhaseRows;
    g_phases.erase(g_phases.begin(), g_phases.begin() + drop);
}

QString phaseLabelKo(const QString& phase) {
    if (phase == QStringLiteral("begin")) {
        return QStringLiteral("토글 시작");
    }
    if (phase == QStringLiteral("undo_snapshot")) {
        return QStringLiteral("실행 취소 스냅샷");
    }
    if (phase == QStringLiteral("undo_copy_profiles")) {
        return QStringLiteral("프로필 폴더 복사(undo)");
    }
    if (phase == QStringLiteral("undo_copy_project_json")) {
        return QStringLiteral("project.json 복사(undo)");
    }
    if (phase == QStringLiteral("set_enabled")) {
        return QStringLiteral("enabled 플래그 변경");
    }
    if (phase == QStringLiteral("configure_row")) {
        return QStringLiteral("목록 행 갱신");
    }
    if (phase == QStringLiteral("feature_enabled_handler")) {
        return QStringLiteral("세션 중지 등 후처리");
    }
    if (phase == QStringLiteral("hotkeys_sync")) {
        return QStringLiteral("단축키 동기화(전체)");
    }
    if (phase == QStringLiteral("hotkeys_fingerprint_skip")) {
        return QStringLiteral("단축키 변경 없음(스킵)");
    }
    if (phase == QStringLiteral("hotkeys_unregister")) {
        return QStringLiteral("단축키 해제");
    }
    if (phase == QStringLiteral("hotkeys_register")) {
        return QStringLiteral("단축키 재등록");
    }
    if (phase == QStringLiteral("project_modified_handler")) {
        return QStringLiteral("자동 저장 예약·워크플로 갱신");
    }
    if (phase == QStringLiteral("pipeline_complete")) {
        return QStringLiteral("토글 완료");
    }
    return phase;
}

QString buildAutoDiagnosis(const std::vector<PhaseRow>& phases,
                           qint64 totalMs,
                           const QString& featureId,
                           bool enabled) {
    QStringList lines;
    lines << QStringLiteral("- **기능**: `%1` → **사용 %2**")
                 .arg(featureId, enabled ? QStringLiteral("켜짐") : QStringLiteral("꺼짐"));

    struct TimedPhase {
        QString phase;
        qint64 durationMs = 0;
    };
    std::vector<TimedPhase> timed;
    timed.reserve(phases.size());
    for (const PhaseRow& row : phases) {
        if (row.durationMs < 0) {
            continue;
        }
        timed.push_back({row.phase, row.durationMs});
    }
    std::sort(timed.begin(), timed.end(), [](const TimedPhase& a, const TimedPhase& b) {
        return a.durationMs > b.durationMs;
    });

    if (!timed.empty()) {
        const TimedPhase& top = timed.front();
        lines << QStringLiteral("- **가장 느린 단계**: `%1` (%2) — **%3 ms**")
                     .arg(top.phase, phaseLabelKo(top.phase))
                     .arg(top.durationMs);
        if (timed.size() > 1) {
            lines << QStringLiteral("- **2위**: `%1` (%2) — %3 ms")
                         .arg(timed[1].phase, phaseLabelKo(timed[1].phase))
                         .arg(timed[1].durationMs);
        }
    }

    lines << QStringLiteral("- **총 기록 시간**: %1 ms").arg(totalMs);

    if (!timed.empty()) {
        const QString& topPhase = timed.front().phase;
        const qint64 topMs = timed.front().durationMs;
        if (topPhase == QStringLiteral("undo_copy_profiles")) {
            lines << QStringLiteral(
                "- **사용 토글 렉 의심**: Ctrl+Z용 **전체 프로필 폴더 복사**가 동기 실행됨 — 이 단계 최적화 우선");
        } else if (topPhase == QStringLiteral("undo_copy_project_json") && topMs >= 15) {
            lines << QStringLiteral(
                "- **사용 토글**: project.json 백업이 %1 ms — 프로젝트가 크면 추가 최적화 검토").arg(topMs);
        } else if (topPhase == QStringLiteral("undo_snapshot") && topMs >= 30) {
            lines << QStringLiteral(
                "- **사용 토글 렉 의심**: 실행 취소 스냅샷 비용 큼 — Phases에서 하위 단계 확인");
        } else if (topPhase.startsWith(QStringLiteral("hotkeys"))) {
            lines << QStringLiteral(
                "- **사용 토글 렉 의심**: 단축키 전체 재등록 — fingerprint 스킵 여부·증분 갱신 검토");
        } else if (topPhase == QStringLiteral("feature_enabled_handler") && topMs >= 50) {
            lines << QStringLiteral(
                "- **사용 토글 렉 의심**: 기능 중지/세션 정리 비용 큼 — 실행 중인 기능을 끌 때 발생할 수 있음");
        } else if (topMs >= 30) {
            lines << QStringLiteral("- **사용 토글**: 체감 지연 가능 — Phases 표에서 duration ms 확인");
        } else {
            lines << QStringLiteral(
                "- **사용 토글**: 단계별 지연은 작음 — **앱 스파이크 진단**으로 GUI 스톨 병행 확인");
        }
    }

    lines << QString();
    lines << QStringLiteral(
        "**AI**: `Phases` 표와 **가장 느린 단계**로 원인 분석. 워크플로 구성을 사용자에게 다시 묻지 마세요.");
    return lines.join(QStringLiteral("\n"));
}

void writeReportToPath(const QString& path, const QString& body) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        file.write(body.toUtf8());
    }
}

} // namespace

bool FeatureToggleProfiler::isEnabled() {
    return ProgramSettings::featureToggleProfiling() || envEnabled();
}

bool FeatureToggleProfiler::isToggleInProgress() {
    return g_toggleActive.load(std::memory_order_relaxed);
}

void FeatureToggleProfiler::beginToggle(const QString& featureId, bool enabled) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_toggleActive.store(true, std::memory_order_relaxed);
    g_featureId = featureId;
    g_enabled = enabled;
    g_toggleStartMs = steadyMs();
    g_phases.clear();
    g_phases.push_back({0,
                        QStringLiteral("begin"),
                        -1,
                        QStringLiteral("%1 → %2")
                            .arg(featureId, enabled ? QStringLiteral("on") : QStringLiteral("off"))});
}

void FeatureToggleProfiler::notePhase(const QString& phase,
                                      qint64 durationMs,
                                      const QString& detail) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_toggleActive.load(std::memory_order_relaxed)) {
        return;
    }
    PhaseRow row;
    row.relMs = relMsLocked();
    row.phase = phase;
    row.durationMs = durationMs;
    row.detail = detail;
    g_phases.push_back(row);
    trimPhasesLocked();
}

void FeatureToggleProfiler::flushReport(const QString& reason) {
    if (!isEnabled()) {
        return;
    }

    QString reportBody;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_toggleActive.load(std::memory_order_relaxed) && g_phases.empty()) {
            return;
        }

        const qint64 totalMs = g_toggleStartMs > 0 ? relMsLocked() : 0;
        const QString version = QCoreApplication::applicationVersion();
        const QStringList paths = allReportPaths();

        reportBody += QStringLiteral("---\n");
        reportBody += QStringLiteral("format: pipbong-feature-toggle\n");
        reportBody += QStringLiteral("format_version: 1\n");
        reportBody += QStringLiteral("end_reason: %1\n").arg(reason);
        reportBody += QStringLiteral("feature_id: %1\n").arg(g_featureId);
        reportBody += QStringLiteral("enabled: %1\n").arg(g_enabled ? QStringLiteral("yes")
                                                                  : QStringLiteral("no"));
        reportBody += QStringLiteral("total_ms: %1\n").arg(totalMs);
        reportBody += QStringLiteral("report_paths: %1\n").arg(paths.join(QStringLiteral("; ")));
        reportBody += QStringLiteral("---\n\n");

        reportBody += QStringLiteral("# Feature toggle report (사용 ON/OFF)\n\n");
        reportBody += QStringLiteral("- Generated: %1\n")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        reportBody += QStringLiteral("- Reason: %1\n").arg(reason);
        reportBody += QStringLiteral("- App version: %1\n").arg(version);
        reportBody += QStringLiteral("- Feature: `%1`\n").arg(g_featureId);
        reportBody += QStringLiteral("- Enabled: %1\n")
                          .arg(g_enabled ? QStringLiteral("on") : QStringLiteral("off"));
        reportBody += QStringLiteral("- Total elapsed: %1 ms\n\n").arg(totalMs);

        reportBody += QStringLiteral("## Auto diagnosis\n\n");
        reportBody += buildAutoDiagnosis(g_phases, totalMs, g_featureId, g_enabled);
        reportBody += QStringLiteral("\n\n");

        reportBody += QStringLiteral("## Phases\n\n");
        reportBody += QStringLiteral("| rel ms | phase | duration ms | detail |\n");
        reportBody += QStringLiteral("| ---: | --- | ---: | --- |\n");
        for (const PhaseRow& row : g_phases) {
            const QString duration =
                row.durationMs >= 0 ? QString::number(row.durationMs) : QStringLiteral("-");
            reportBody += QStringLiteral("| %1 | %2 | %3 | %4 |\n")
                              .arg(row.relMs)
                              .arg(row.phase)
                              .arg(duration)
                              .arg(row.detail);
        }
        reportBody += QStringLiteral("\n");

        g_toggleActive.store(false, std::memory_order_relaxed);
        g_phases.clear();
        g_featureId.clear();
        g_enabled = true;
        g_toggleStartMs = 0;
    }

    for (const QString& path : allReportPaths()) {
        writeReportToPath(path, reportBody);
    }
}

QString FeatureToggleProfiler::latestReportPath() {
    const QString repo = repoOutputDirectory();
    if (!repo.isEmpty()) {
        return QDir(repo).filePath(QStringLiteral("latest.md"));
    }
    return QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
}

QStringList FeatureToggleProfiler::allReportPaths() {
    QStringList paths;
    const QString repo = repoOutputDirectory();
    if (!repo.isEmpty()) {
        paths << QDir(repo).filePath(QStringLiteral("latest.md"));
    }
    paths << QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
    paths.removeDuplicates();
    return paths;
}
