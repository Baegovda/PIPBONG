#include "core/diagnostics/ProfileSwitchProfiler.h"

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

constexpr int kMaxPhaseRows = 256;

struct PhaseRow {
    qint64 relMs = 0;
    QString phase;
    qint64 durationMs = -1;
    QString detail;
};

std::mutex g_mutex;
std::atomic<bool> g_switchActive{false};
QString g_targetProfileId;
QString g_switchMode;
qint64 g_switchStartMs = 0;
int g_pingpongCount = 0;
int g_startTimerWarnings = 0;
std::vector<PhaseRow> g_phases;

qint64 steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

qint64 relMsLocked() {
    if (g_switchStartMs <= 0) {
        return 0;
    }
    return steadyMs() - g_switchStartMs;
}

bool envEnabled() {
    return qEnvironmentVariableIsSet("PIPBONG_PROFILE_SWITCH_PROFILE")
           && qEnvironmentVariable("PIPBONG_PROFILE_SWITCH_PROFILE") == QLatin1String("1");
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
    return QDir(local).filePath(QStringLiteral("profile-switch"));
}

QString repoOutputDirectory() {
    const QString repo = findRepoRootDirectory();
    if (repo.isEmpty()) {
        return QString();
    }
    return QDir(repo).filePath(QStringLiteral("profile-switch"));
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
        return QStringLiteral("전환 시작");
    }
    if (phase == QStringLiteral("requested")) {
        return QStringLiteral("자동 전환 요청");
    }
    if (phase == QStringLiteral("stable")) {
        return QStringLiteral("포그라운드 안정화");
    }
    if (phase == QStringLiteral("ui_selection")) {
        return QStringLiteral("프로필 목록 선택 갱신");
    }
    if (phase == QStringLiteral("maybe_save")) {
        return QStringLiteral("저장 확인");
    }
    if (phase == QStringLiteral("save_profile_settings")) {
        return QStringLiteral("프로필 설정 저장");
    }
    if (phase == QStringLiteral("sessions_stopped")) {
        return QStringLiteral("실행 세션 중지");
    }
    if (phase == QStringLiteral("manifest_saved")) {
        return QStringLiteral("활성 프로필 변경");
    }
    if (phase == QStringLiteral("load_settings")) {
        return QStringLiteral("프로필 설정 로드");
    }
    if (phase == QStringLiteral("center_pin_apply")) {
        return QStringLiteral("창 중앙 고정 적용");
    }
    if (phase == QStringLiteral("project_cache_hit")) {
        return QStringLiteral("프로젝트 캐시 적중");
    }
    if (phase == QStringLiteral("json_deserialize")) {
        return QStringLiteral("project.json 파싱");
    }
    if (phase == QStringLiteral("prepare_unload")) {
        return QStringLiteral("이전 프로젝트 언로드");
    }
    if (phase == QStringLiteral("feature_list_bind")) {
        return QStringLiteral("기능 목록 바인딩");
    }
    if (phase == QStringLiteral("workflow_refresh")) {
        return QStringLiteral("워크플로우 패널 갱신");
    }
    if (phase == QStringLiteral("sync_hotkeys")) {
        return QStringLiteral("단축키 동기화");
    }
    if (phase == QStringLiteral("target_details")) {
        return QStringLiteral("대상 창 패널 갱신");
    }
    if (phase == QStringLiteral("project_loaded")) {
        return QStringLiteral("프로젝트 로드(전체)");
    }
    if (phase == QStringLiteral("post_load_sync")) {
        return QStringLiteral("로드 후 동기화");
    }
    if (phase == QStringLiteral("triggers_restored")) {
        return QStringLiteral("트리거 감시 복원");
    }
    if (phase == QStringLiteral("target_activation")) {
        return QStringLiteral("대상 창 포커스 이동");
    }
    if (phase == QStringLiteral("run_warmup_scheduled")) {
        return QStringLiteral("실행 워밍업 예약");
    }
    if (phase == QStringLiteral("pipeline_complete")) {
        return QStringLiteral("전환 파이프라인 완료");
    }
    return phase;
}

QString buildAutoDiagnosis(const std::vector<PhaseRow>& phases,
                           qint64 totalMs,
                           const QString& mode,
                           int pingpongCount) {
    QStringList lines;
    lines << QStringLiteral("- **전환 방식**: %1")
                 .arg(mode == QStringLiteral("manual") ? QStringLiteral("수동 (프로필 목록 클릭)")
                                                       : QStringLiteral("자동 (포그라운드 창 매칭)"));

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

    lines << QStringLiteral("- **총 기록 시간**: %1 ms (committed 시점)").arg(totalMs);

    if (mode == QStringLiteral("manual")) {
        if (!timed.empty() && timed.front().durationMs >= 80) {
            const QString& p = timed.front().phase;
            if (p == QStringLiteral("workflow_refresh") || p == QStringLiteral("project_loaded")
                || p == QStringLiteral("json_deserialize") || p == QStringLiteral("feature_list_bind")) {
                lines << QStringLiteral(
                    "- **수동 전환 렉 의심**: UI 스레드에서 프로젝트/워크플로우 갱신이 큼 — 해당 단계 최적화 우선");
            } else if (p == QStringLiteral("sessions_stopped")) {
                lines << QStringLiteral(
                    "- **수동 전환 렉 의심**: 실행 중인 기능 세션 중지 대기 — 워커 정지 시간 확인");
            } else if (p == QStringLiteral("target_activation")) {
                lines << QStringLiteral(
                    "- **수동 전환 렉 의심**: 대상 창 포커스 이동(설정 켜짐) — 필요 시 **프로필 선택 시 대상 창으로 포커스 이동** 끄기");
            }
        } else if (totalMs >= 50) {
            lines << QStringLiteral("- **수동 전환**: 체감 지연 가능 — 아래 Phases 표에서 duration ms 확인");
        } else {
            lines << QStringLiteral("- **수동 전환**: 기록된 단계별 지연은 작음 — AppSpikeProfiler로 GUI 스톨 병행 확인");
        }
    }

    if (pingpongCount > 2) {
        lines << QStringLiteral("- **핑퐁**: 요청 %1회 — 자동 전환 안정화/최소 간격 확인").arg(pingpongCount);
    }

    lines << QString();
    lines << QStringLiteral(
        "**AI**: `Phases` 표의 duration ms와 위 **가장 느린 단계**로 원인 분석. 사용자에게 워크플로 구성을 다시 묻지 마세요.");
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

bool ProfileSwitchProfiler::isEnabled() {
    return ProgramSettings::profileSwitchProfiling() || envEnabled();
}

bool ProfileSwitchProfiler::isSwitchInProgress() {
    return g_switchActive.load(std::memory_order_relaxed);
}

void ProfileSwitchProfiler::beginSwitch(const QString& targetProfileId, const QString& mode) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_switchActive.store(true, std::memory_order_relaxed);
    g_targetProfileId = targetProfileId;
    g_switchMode = mode;
    g_switchStartMs = steadyMs();
    g_pingpongCount = 0;
    g_startTimerWarnings = 0;
    g_phases.clear();
    g_phases.push_back({0, QStringLiteral("begin"), -1, mode + QStringLiteral(" → ") + targetProfileId});
}

void ProfileSwitchProfiler::notePhase(const QString& phase,
                                      qint64 durationMs,
                                      const QString& detail) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_switchActive.load(std::memory_order_relaxed) && phase != QStringLiteral("requested")) {
        g_switchActive.store(true, std::memory_order_relaxed);
        g_switchStartMs = steadyMs();
        g_phases.clear();
    }
    PhaseRow row;
    row.relMs = relMsLocked();
    row.phase = phase;
    row.durationMs = durationMs;
    row.detail = detail;
    g_phases.push_back(row);
    trimPhasesLocked();
}

void ProfileSwitchProfiler::notePingpong() {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_pingpongCount;
}

void ProfileSwitchProfiler::incrementStartTimerWarningCount() {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_startTimerWarnings;
}

QString ProfileSwitchProfiler::latestReportPath() {
    const QString repo = repoOutputDirectory();
    if (!repo.isEmpty()) {
        return QDir(repo).filePath(QStringLiteral("latest.md"));
    }
    return QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
}

QStringList ProfileSwitchProfiler::allReportPaths() {
    QStringList paths;
    const QString repo = repoOutputDirectory();
    if (!repo.isEmpty()) {
        paths << QDir(repo).filePath(QStringLiteral("latest.md"));
    }
    paths << QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
    paths.removeDuplicates();
    return paths;
}

void ProfileSwitchProfiler::flushReport(const QString& reason) {
    if (!isEnabled()) {
        return;
    }

    QString reportBody;
  {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_switchActive.load(std::memory_order_relaxed) && g_phases.empty()) {
            return;
        }

        const qint64 totalMs = g_switchStartMs > 0 ? relMsLocked() : 0;
        const QString version = QCoreApplication::applicationVersion();
        const QStringList paths = allReportPaths();

        reportBody += QStringLiteral("---\n");
        reportBody += QStringLiteral("format: pipbong-profile-switch\n");
        reportBody += QStringLiteral("format_version: 2\n");
        reportBody += QStringLiteral("end_reason: %1\n").arg(reason);
        reportBody += QStringLiteral("switch_mode: %1\n").arg(g_switchMode);
        reportBody += QStringLiteral("total_ms: %1\n").arg(totalMs);
        reportBody += QStringLiteral("report_paths: %1\n").arg(paths.join(QStringLiteral("; ")));
        reportBody += QStringLiteral("---\n\n");

        reportBody += QStringLiteral("# Profile switch report\n\n");
        reportBody += QStringLiteral("- Generated: %1\n")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        reportBody += QStringLiteral("- Reason: %1\n").arg(reason);
        reportBody += QStringLiteral("- App version: %1\n").arg(version);
        reportBody += QStringLiteral("- Switch mode: `%1`\n").arg(g_switchMode);
        if (!g_targetProfileId.isEmpty()) {
            reportBody += QStringLiteral("- Target profile: `%1`\n").arg(g_targetProfileId);
        }
        reportBody += QStringLiteral("- Total elapsed: %1 ms\n").arg(totalMs);
        reportBody += QStringLiteral("- Ping-pong requests: %1\n").arg(g_pingpongCount);
        reportBody += QStringLiteral("- startTimer cross-thread warnings: %1\n\n")
                          .arg(g_startTimerWarnings);

        reportBody += QStringLiteral("## Auto diagnosis\n\n");
        reportBody += buildAutoDiagnosis(g_phases, totalMs, g_switchMode, g_pingpongCount);
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

        g_switchActive.store(false, std::memory_order_relaxed);
        g_phases.clear();
        g_targetProfileId.clear();
        g_switchMode.clear();
        g_switchStartMs = 0;
        g_pingpongCount = 0;
        g_startTimerWarnings = 0;
    }

    for (const QString& path : allReportPaths()) {
        writeReportToPath(path, reportBody);
    }
}
