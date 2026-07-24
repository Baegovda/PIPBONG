#include "core/diagnostics/CursorStutterProfiler.h"

#include "app/MouseCenterLock.h"
#include "app/ProgramSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

Q_LOGGING_CATEGORY(lcCursorStutter, "pipbong.cursor_stutter")

namespace {

constexpr int kSamplerIntervalMs = 12;
constexpr int kJumpThresholdPx = 8;
constexpr int kMicroJumpThresholdPx = 3;
constexpr int kSnapBackWindowUs = 50000;
constexpr int kSnapBackMinPx = 4;
constexpr qint64 kSamplerOverrunUs = 12000;
constexpr qint64 kPeriodicFlushUs = 30'000'000;
constexpr int kMaxEvents = 120000;
constexpr int kMaxJumpRows = 120;

bool g_verbose = false;
std::mutex g_mutex;
bool g_sampling = false;
bool g_hasLastSample = false;
int g_lastSampleX = 0;
int g_lastSampleY = 0;
qint64 g_sessionStartUs = 0;
qint64 g_lastPeriodicFlushUs = 0;
qint64 g_samplerTickCount = 0;

std::thread g_samplerThread;
std::atomic<bool> g_stopSampler{false};

struct EventRow {
    qint64 relUs = 0;
    const char* threadTag = "main";
    const char* kind = "";
    QString detail;
    qint64 durationUs = -1;
};

std::vector<EventRow> g_events;

struct CounterRow {
    QString key;
    qint64 count = 0;
};

std::vector<CounterRow> g_setCursorByCaller;
std::vector<CounterRow> g_clipByCaller;
qint64 g_mouseHookSnapCount = 0;
qint64 g_jumpCount = 0;
qint64 g_microJumpCount = 0;
qint64 g_snapBackCount = 0;
qint64 g_samplerOverrunCount = 0;
qint64 g_qwerPhysicalDown = 0;
qint64 g_qwerSynthetic = 0;
qint64 g_jumpsNearQwerMs = 0;
qint64 g_jumpsNearHoldFeature = 0;
qint64 g_slowKeyboardHookCount = 0;
qint64 g_holdFeatureStartCount = 0;
qint64 g_foregroundChangeCount = 0;
quintptr g_lastForegroundHwnd = static_cast<quintptr>(-1);
QString g_cachedForegroundDetail = QStringLiteral("fg=(unknown)");

struct RecentMove {
    qint64 relUs = 0;
    int fromX = 0;
    int fromY = 0;
    int toX = 0;
    int toY = 0;
};

RecentMove g_lastMove{};

bool isAlwaysRecordedKind(const char* kind) {
    return std::strcmp(kind, "cursor_jump") == 0 || std::strcmp(kind, "micro_jump") == 0
           || std::strcmp(kind, "snap_back") == 0 || std::strcmp(kind, "sampler_overrun") == 0
           || std::strcmp(kind, "hold_feature") == 0 || std::strcmp(kind, "foreground_focus") == 0;
}

bool isQwerVirtualKey(int vk) {
    switch (vk) {
    case 0x51:
    case 0x57:
    case 0x45:
    case 0x52:
        return true;
    default:
        return false;
    }
}

const char* qwerLabel(int vk) {
    switch (vk) {
    case 0x51:
        return "Q";
    case 0x57:
        return "W";
    case 0x45:
        return "E";
    case 0x52:
        return "R";
    default:
        return "?";
    }
}

bool isLolHoldFeatureName(const QString& name) {
    return name == QLatin1String("Q") || name == QLatin1String("W") || name == QLatin1String("E")
           || name == QLatin1String("R");
}

qint64 steadyUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

qint64 relUsLocked() {
    if (g_sessionStartUs <= 0) {
        return 0;
    }
    return steadyUs() - g_sessionStartUs;
}

void bumpCounter(std::vector<CounterRow>& table, const QString& key) {
    for (CounterRow& row : table) {
        if (row.key == key) {
            ++row.count;
            return;
        }
    }
    table.push_back({key, 1});
}

void pushEventLocked(const char* kind,
                     const QString& detail,
                     qint64 durationUs = -1,
                     const char* threadTag = "main") {
    if (!g_verbose && !isAlwaysRecordedKind(kind)) {
        return;
    }
    if (static_cast<int>(g_events.size()) >= kMaxEvents) {
        g_events.erase(g_events.begin(), g_events.begin() + (g_events.size() / 10));
    }
    g_events.push_back({relUsLocked(), threadTag, kind, detail, durationUs});
}

bool envVerboseEnabled() {
#ifdef _WIN32
    wchar_t buffer[16]{};
    if (GetEnvironmentVariableW(L"PIPBONG_CURSOR_STUTTER_PROFILE", buffer, 16) > 0) {
        return buffer[0] == L'1';
    }
#endif
    return false;
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

QString appDataReportDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/AppData/Local/PIPBONG/PIPBONG");
    }
    return QDir(base).absoluteFilePath(QStringLiteral("cursor-stutter"));
}

QString lockStateDetail() {
#ifdef _WIN32
    if (!MouseCenterLock::isActive()) {
        return QStringLiteral("lock=off");
    }
    if (MouseCenterLock::isAnchoredToTargetWindow()) {
        return QStringLiteral("lock=target_anchor");
    }
    return QStringLiteral("lock=fixed_point");
#else
    return QStringLiteral("lock=unknown");
#endif
}

QString foregroundWindowDetail(HWND hwnd = nullptr) {
#ifdef _WIN32
    if (!hwnd) {
        hwnd = GetForegroundWindow();
    }
    if (!hwnd || !IsWindow(hwnd)) {
        return QStringLiteral("fg=(none)");
    }

    wchar_t titleBuffer[512]{};
    GetWindowTextW(hwnd, titleBuffer, 512);
    QString title = QString::fromWCharArray(titleBuffer).trimmed();
    if (title.isEmpty()) {
        title = QStringLiteral("(empty title)");
    }
    title.replace(QLatin1Char('"'), QLatin1String("'"));

    QString exeName = QStringLiteral("?");
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != 0) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (process) {
            wchar_t pathBuffer[MAX_PATH]{};
            DWORD pathLength = MAX_PATH;
            if (QueryFullProcessImageNameW(process, 0, pathBuffer, &pathLength)) {
                exeName = QFileInfo(QString::fromWCharArray(pathBuffer)).fileName();
            }
            CloseHandle(process);
        }
    }

    return QStringLiteral("fg=\"%1\" hwnd=0x%2 exe=%3")
        .arg(title)
        .arg(reinterpret_cast<quintptr>(hwnd), 0, 16)
        .arg(exeName);
#else
    (void)hwnd;
    return QStringLiteral("fg=unknown");
#endif
}

QString appendForegroundContext(const QString& base) {
    return base + QLatin1Char(' ') + g_cachedForegroundDetail;
}

void refreshForegroundCacheLocked(HWND hwnd = nullptr) {
#ifdef _WIN32
    g_cachedForegroundDetail = foregroundWindowDetail(hwnd);
#else
    (void)hwnd;
    g_cachedForegroundDetail = QStringLiteral("fg=unknown");
#endif
}

void pollForegroundWindowChange() {
#ifdef _WIN32
    HWND fg = GetForegroundWindow();
    if (!fg || !IsWindow(fg)) {
        fg = nullptr;
    }
    const quintptr hwndVal = reinterpret_cast<quintptr>(fg);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (hwndVal == g_lastForegroundHwnd) {
        return;
    }
    g_lastForegroundHwnd = hwndVal;
    ++g_foregroundChangeCount;
    refreshForegroundCacheLocked(fg);
    pushEventLocked("foreground_focus", g_cachedForegroundDetail, -1, "sampler");
#endif
}

void correlateJumpWithRecentEventsLocked() {
    const qint64 windowStart = relUsLocked() - 100000;
    bool nearQwer = false;
    bool nearHold = false;
    int scanned = 0;
    constexpr int kMaxCorrelateScan = 48;
    for (auto it = g_events.rbegin(); it != g_events.rend() && scanned < kMaxCorrelateScan; ++it, ++scanned) {
        if (it->relUs < windowStart) {
            break;
        }
        if (std::strcmp(it->kind, "physical_key") == 0 || std::strcmp(it->kind, "synthetic_key") == 0) {
            if (it->detail == QLatin1String("Q") || it->detail == QLatin1String("W")
                || it->detail == QLatin1String("E") || it->detail == QLatin1String("R")
                || it->detail.contains(QLatin1String("qwer="))) {
                nearQwer = true;
            }
        }
        if (std::strcmp(it->kind, "hold_feature") == 0) {
            const QStringList parts = it->detail.split(QLatin1Char(' '));
            if (!parts.isEmpty() && isLolHoldFeatureName(parts.front())) {
                nearHold = true;
            }
        }
        if (std::strcmp(it->kind, "set_cursor") == 0 || std::strcmp(it->kind, "mouse_hook_snap") == 0) {
            nearHold = true;
        }
    }
    if (nearQwer) {
        ++g_jumpsNearQwerMs;
    }
    if (nearHold) {
        ++g_jumpsNearHoldFeature;
    }
}

void maybeDetectSnapBackLocked(int fromX, int fromY, int toX, int toY, int distancePx) {
    if (!g_lastMove.relUs) {
        return;
    }
    const qint64 now = relUsLocked();
    if (now - g_lastMove.relUs > kSnapBackWindowUs) {
        return;
    }
    const int backDx = toX - g_lastMove.fromX;
    const int backDy = toY - g_lastMove.fromY;
    const int backDist =
        static_cast<int>(std::lround(std::sqrt(static_cast<double>(backDx * backDx + backDy * backDy))));
    if (backDist < kSnapBackMinPx) {
        return;
    }
    const int priorDx = g_lastMove.toX - g_lastMove.fromX;
    const int priorDy = g_lastMove.toY - g_lastMove.fromY;
    if (priorDx * backDx < 0 || priorDy * backDy < 0) {
        ++g_snapBackCount;
        pushEventLocked("snap_back",
                        appendForegroundContext(QStringLiteral("A=(%1,%2) B=(%3,%4) C=(%5,%6) dist=%7px %8")
                                                .arg(g_lastMove.fromX)
                                                .arg(g_lastMove.fromY)
                                                .arg(g_lastMove.toX)
                                                .arg(g_lastMove.toY)
                                                .arg(toX)
                                                .arg(toY)
                                                .arg(distancePx)
                                                .arg(lockStateDetail())),
                        -1,
                        "sampler");
    }
}

void noteJumpLocked(int fromX,
                    int fromY,
                    int toX,
                    int toY,
                    int distancePx,
                    const char* source,
                    bool micro) {
    if (micro) {
        ++g_microJumpCount;
        // Sampler micro-jumps are mostly normal mouse movement — count only, no event row.
        if (std::strcmp(source, "sampler") == 0) {
            g_lastMove = {relUsLocked(), fromX, fromY, toX, toY};
            return;
        }
    } else {
        ++g_jumpCount;
        // Sampler jumps are normal mouse movement — counters only, no correlate/scan.
        if (std::strcmp(source, "sampler") == 0) {
            g_lastMove = {relUsLocked(), fromX, fromY, toX, toY};
            return;
        }
    }
    const char* kind = micro ? "micro_jump" : "cursor_jump";
    const QString detail = appendForegroundContext(QStringLiteral("%1 from=(%2,%3) to=(%4,%5) dist=%6px %7")
                               .arg(QString::fromUtf8(source))
                               .arg(fromX)
                               .arg(fromY)
                               .arg(toX)
                               .arg(toY)
                               .arg(distancePx)
                               .arg(lockStateDetail()));
    pushEventLocked(kind, detail, -1, "sampler");
    if (!micro) {
        correlateJumpWithRecentEventsLocked();
        maybeDetectSnapBackLocked(fromX, fromY, toX, toY, distancePx);
    }
    g_lastMove = {relUsLocked(), fromX, fromY, toX, toY};
}

void noteSamplerOverrunLocked(qint64 gapUs) {
    ++g_samplerOverrunCount;
    pushEventLocked("sampler_overrun",
                    appendForegroundContext(
                        QStringLiteral("gap_us=%1 expected_ms=%2").arg(gapUs).arg(kSamplerIntervalMs)),
                    gapUs,
                    "sampler");
}

void sampleCursorPosition() {
#ifdef _WIN32
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_samplerTickCount;
    if (g_hasLastSample) {
        const int dx = pt.x - g_lastSampleX;
        const int dy = pt.y - g_lastSampleY;
        const int dist = static_cast<int>(std::lround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
        if (dist >= kJumpThresholdPx) {
            noteJumpLocked(g_lastSampleX, g_lastSampleY, pt.x, pt.y, dist, "sampler", false);
        } else if (dist >= kMicroJumpThresholdPx) {
            noteJumpLocked(g_lastSampleX, g_lastSampleY, pt.x, pt.y, dist, "sampler", true);
        }
    }
    g_lastSampleX = pt.x;
    g_lastSampleY = pt.y;
    g_hasLastSample = true;
#else
    (void)0;
#endif
}

QString buildMarkdownReport(const QString& endReason) {
    QString foregroundAtEnd;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        foregroundAtEnd = g_cachedForegroundDetail;
    }
    std::lock_guard<std::mutex> lock(g_mutex);

    QStringList lines;
    lines << QStringLiteral("---");
    lines << QStringLiteral("format: pipbong-cursor-stutter");
    lines << QStringLiteral("format_version: 3");
    lines << QStringLiteral("end_reason: %1").arg(endReason);
    lines << QStringLiteral("session_end: %1")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    lines << QStringLiteral("sampler_running: %1").arg(g_sampling ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("verbose_logging: %1").arg(g_verbose ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("sampler_ticks: %1").arg(g_samplerTickCount);
    lines << QStringLiteral("jump_threshold_px: %1").arg(kJumpThresholdPx);
    lines << QStringLiteral("micro_jump_threshold_px: %1").arg(kMicroJumpThresholdPx);
    lines << QStringLiteral("sampler_interval_ms: %1").arg(kSamplerIntervalMs);
    lines << QStringLiteral("events_recorded: %1").arg(g_events.size());
    lines << QStringLiteral("foreground_at_end: %1").arg(foregroundAtEnd);
    lines << QStringLiteral("foreground_changes: %1").arg(g_foregroundChangeCount);
    const QStringList paths = CursorStutterProfiler::allReportPaths();
    lines << QStringLiteral("report_paths: %1").arg(paths.join(QLatin1String("; ")));
    lines << QStringLiteral("---");
    lines << QString();
    lines << QStringLiteral("# Cursor stutter report");
    lines << QString();

    lines << QStringLiteral("## Auto diagnosis");
    lines << QString();
    lines << QStringLiteral("- **종료 시 포커스 창**: %1").arg(foregroundAtEnd);
    lines << QString();
    if (g_samplerTickCount == 0) {
        lines << QStringLiteral(
            "- **샘플러 미동작**: 백그라운드 커서 샘플러가 한 번도 돌지 않았습니다 — 빌드/실행 경로를 확인하세요.");
    } else if (g_jumpCount == 0 && g_microJumpCount == 0 && g_snapBackCount == 0) {
        lines << QStringLiteral(
            "- **커서 점프**: 샘플러 %1회 틱 기준 ≥%2 px 점프·≥%3 px 미세 점프·snap-back 없음 — 재현 구간에 앱이 실행 중이었는지 확인.")
                    .arg(g_samplerTickCount)
                    .arg(kJumpThresholdPx)
                    .arg(kMicroJumpThresholdPx);
    } else {
        if (g_jumpCount > 0) {
            lines << QStringLiteral("- **커서 점프 (≥%1 px)**: %2회").arg(kJumpThresholdPx).arg(g_jumpCount);
        }
        if (g_microJumpCount > 0) {
            lines << QStringLiteral("- **미세 점프 (%1–%2 px)**: %3회")
                        .arg(kMicroJumpThresholdPx)
                        .arg(kJumpThresholdPx - 1)
                        .arg(g_microJumpCount);
        }
        if (g_snapBackCount > 0) {
            lines << QStringLiteral(
                "- **스냅백 (왕복 튐)**: %1회 — SetCursorPos·마우스 잠금 훅·게임 클램프 충돌 의심")
                        .arg(g_snapBackCount);
        }
        if (g_jumpsNearHoldFeature > 0) {
            lines << QStringLiteral(
                "- **Hold 기능 상관**: 점프 %1회가 직전 100 ms 내 Q/W/E/R Hold 기능·SetCursorPos·훅 스냅과 겹침")
                        .arg(g_jumpsNearHoldFeature);
        }
        if (g_jumpsNearQwerMs > 0) {
            lines << QStringLiteral(
                "- **QWER 키 상관**: 점프 %1회가 직전 100 ms 내 Q/W/E/R 물리·합성 키와 겹침")
                        .arg(g_jumpsNearQwerMs);
        }
    }
    if (g_samplerOverrunCount > 0) {
        lines << QStringLiteral(
            "- **샘플러 지연**: %1회 >%2 ms 간격 — UI/훅 스레드 정지로 커서 튐이 샘플링에 누락됐을 수 있음")
                    .arg(g_samplerOverrunCount)
                    .arg(kSamplerOverrunUs / 1000);
    }
    if (g_mouseHookSnapCount > 0) {
        lines << QStringLiteral(
            "- **마우스 잠금 훅**: `mouse_hook_snap` %1회 — MouseCenterLock 이 물리 이동을 되돌림")
                    .arg(g_mouseHookSnapCount);
    }
    if (!g_setCursorByCaller.empty()) {
        std::sort(g_setCursorByCaller.begin(),
                  g_setCursorByCaller.end(),
                  [](const CounterRow& a, const CounterRow& b) { return a.count > b.count; });
        lines << QStringLiteral("- **SetCursorPos**: 최다 `%1` (%2회)")
                    .arg(g_setCursorByCaller.front().key)
                    .arg(g_setCursorByCaller.front().count);
    }
    if (g_slowKeyboardHookCount > 0) {
        lines << QStringLiteral("- **느린 키보드 훅**: %1회 >1 ms").arg(g_slowKeyboardHookCount);
    }
    lines << QStringLiteral("- **Hold 기능 시작**: %1회 · **물리 QWER**: %2 · **합성 QWER**: %3")
                .arg(g_holdFeatureStartCount)
                .arg(g_qwerPhysicalDown)
                .arg(g_qwerSynthetic);
    lines << QString();
    lines << QStringLiteral(
        "**AI**: `cursor_jump` / `micro_jump` / `snap_back` / `sampler_overrun` / `hold_feature` / `set_cursor` / `mouse_hook_snap` 로 원인 분석. 워크플로 구성을 사용자에게 다시 묻지 마세요.");
    lines << QString();

    auto emitCounterTable = [&](const char* title, const std::vector<CounterRow>& rows) {
        if (rows.empty()) {
            return;
        }
        std::vector<CounterRow> sorted = rows;
        std::sort(sorted.begin(), sorted.end(), [](const CounterRow& a, const CounterRow& b) {
            return a.count > b.count;
        });
        lines << QStringLiteral("## %1").arg(QString::fromUtf8(title));
        lines << QString();
        lines << QStringLiteral("| Caller | count |");
        lines << QStringLiteral("| --- | ---: |");
        const int limit = std::min<int>(20, static_cast<int>(sorted.size()));
        for (int i = 0; i < limit; ++i) {
            lines << QStringLiteral("| %1 | %2 |")
                         .arg(sorted[static_cast<size_t>(i)].key)
                         .arg(sorted[static_cast<size_t>(i)].count);
        }
        lines << QString();
    };

    emitCounterTable("SetCursorPos by caller", g_setCursorByCaller);
    emitCounterTable("ClipCursor by caller", g_clipByCaller);

    lines << QStringLiteral("## Summary counters");
    lines << QString();
    lines << QStringLiteral("| Metric | value |");
    lines << QStringLiteral("| --- | ---: |");
    lines << QStringLiteral("| sampler_ticks | %1 |").arg(g_samplerTickCount);
    lines << QStringLiteral("| cursor_jump | %1 |").arg(g_jumpCount);
    lines << QStringLiteral("| micro_jump | %1 |").arg(g_microJumpCount);
    lines << QStringLiteral("| snap_back | %1 |").arg(g_snapBackCount);
    lines << QStringLiteral("| sampler_overrun | %1 |").arg(g_samplerOverrunCount);
    lines << QStringLiteral("| mouse_hook_snap | %1 |").arg(g_mouseHookSnapCount);
    lines << QStringLiteral("| hold_feature_start | %1 |").arg(g_holdFeatureStartCount);
    lines << QStringLiteral("| physical_qwer | %1 |").arg(g_qwerPhysicalDown);
    lines << QStringLiteral("| synthetic_qwer | %1 |").arg(g_qwerSynthetic);
    lines << QStringLiteral("| jumps_near_hold_100ms | %1 |").arg(g_jumpsNearHoldFeature);
    lines << QStringLiteral("| jumps_near_qwer_100ms | %1 |").arg(g_jumpsNearQwerMs);
    lines << QStringLiteral("| slow_keyboard_hook_gt_1ms | %1 |").arg(g_slowKeyboardHookCount);
    lines << QStringLiteral("| foreground_focus_changes | %1 |").arg(g_foregroundChangeCount);
    lines << QString();

    auto emitJumpTable = [&](const char* title, const char* kind) {
        lines << QStringLiteral("## %1").arg(QString::fromUtf8(title));
        lines << QString();
        lines << QStringLiteral("| rel_ms | detail |");
        lines << QStringLiteral("| ---: | --- |");
        int jumpRows = 0;
        for (auto it = g_events.rbegin(); it != g_events.rend() && jumpRows < kMaxJumpRows; ++it) {
            if (std::strcmp(it->kind, kind) != 0) {
                continue;
            }
            lines << QStringLiteral("| %1 | %2 |")
                         .arg(QString::number(static_cast<double>(it->relUs) / 1000.0, 'f', 3))
                         .arg(it->detail);
            ++jumpRows;
        }
        if (jumpRows == 0) {
            lines << QStringLiteral("| — | (none) |");
        }
        lines << QString();
    };

    emitJumpTable("Recent cursor jumps (newest last)", "cursor_jump");
    emitJumpTable("Recent micro jumps", "micro_jump");
    emitJumpTable("Recent snap-back", "snap_back");

    lines << QStringLiteral("## Recent foreground focus (newest last)");
    lines << QString();
    lines << QStringLiteral("| rel_ms | detail |");
    lines << QStringLiteral("| ---: | --- |");
    int fgRows = 0;
    for (auto it = g_events.rbegin(); it != g_events.rend() && fgRows < 40; ++it) {
        if (std::strcmp(it->kind, "foreground_focus") != 0) {
            continue;
        }
        lines << QStringLiteral("| %1 | %2 |")
                     .arg(QString::number(static_cast<double>(it->relUs) / 1000.0, 'f', 3))
                     .arg(it->detail);
        ++fgRows;
    }
    if (fgRows == 0) {
        lines << QStringLiteral("| — | (none) |");
    }
    lines << QString();

    lines << QStringLiteral("## Event log (tsv)");
    lines << QString();
    lines << QStringLiteral("```tsv");
    lines << QStringLiteral("rel_us\tthread\tevent\tdetail\tdur_us");
    const int tail = std::min<int>(2000, static_cast<int>(g_events.size()));
    for (int i = static_cast<int>(g_events.size()) - tail; i < static_cast<int>(g_events.size()); ++i) {
        if (i < 0) {
            continue;
        }
        const EventRow& row = g_events[static_cast<size_t>(i)];
        lines << QStringLiteral("%1\t%2\t%3\t%4\t%5")
                     .arg(row.relUs)
                     .arg(QString::fromUtf8(row.threadTag))
                     .arg(QString::fromUtf8(row.kind))
                     .arg(row.detail)
                     .arg(row.durationUs);
    }
    lines << QStringLiteral("```");
    lines << QString();

    return lines.join(QLatin1Char('\n'));
}

bool writeReportToPaths(const QString& content, const QString& reason) {
    bool anyWritten = false;
    for (const QString& path : CursorStutterProfiler::allReportPaths()) {
        const QFileInfo info(path);
        QDir().mkpath(info.absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            qCWarning(lcCursorStutter) << "Failed to write cursor stutter report:" << path
                                       << file.errorString();
            continue;
        }
        file.write(content.toUtf8());
        anyWritten = true;
        qCInfo(lcCursorStutter) << "Wrote cursor stutter report (" << reason << "):" << path;
    }
    if (!anyWritten) {
        qCWarning(lcCursorStutter) << "No cursor stutter report path writable for reason:" << reason;
    }
    return anyWritten;
}

void samplerLoop() {
    qint64 lastTickUs = steadyUs();
    while (!g_stopSampler.load(std::memory_order_relaxed)) {
        const qint64 tickStart = steadyUs();
        const qint64 gapUs = tickStart - lastTickUs;
        if (lastTickUs > 0 && gapUs > kSamplerOverrunUs) {
            std::lock_guard<std::mutex> lock(g_mutex);
            noteSamplerOverrunLocked(gapUs);
        }
        lastTickUs = tickStart;

        pollForegroundWindowChange();
        sampleCursorPosition();

        std::this_thread::sleep_for(std::chrono::milliseconds(kSamplerIntervalMs));
    }
}

} // namespace

void stopSamplerThread() {
    if (!g_sampling) {
        return;
    }
    g_stopSampler.store(true, std::memory_order_relaxed);
    if (g_samplerThread.joinable()) {
        g_samplerThread.join();
    }
    g_sampling = false;
}

bool CursorStutterProfiler::isVerboseLogging() {
    return g_verbose;
}

void CursorStutterProfiler::reloadFromSettings() {
    const bool wasVerbose = g_verbose;
    g_verbose = ProgramSettings::cursorStutterProfiling() || envVerboseEnabled();
    if (g_sessionStartUs <= 0) {
        g_sessionStartUs = steadyUs();
    }
    if (g_verbose) {
        refreshForegroundCacheLocked();
        startSampling();
    } else {
        stopSamplerThread();
        if (wasVerbose) {
            flushReport(QStringLiteral("verbose_disabled"));
        }
    }
}

QString CursorStutterProfiler::outputDirectory() {
    const QString repo = findRepoRootDirectory();
    if (!repo.isEmpty()) {
        return QDir(repo).absoluteFilePath(QStringLiteral("cursor-stutter"));
    }
    return appDataReportDirectory();
}

QString CursorStutterProfiler::latestReportPath() {
    return QDir(outputDirectory()).absoluteFilePath(QStringLiteral("latest.md"));
}

QStringList CursorStutterProfiler::allReportPaths() {
    QStringList paths;
    const QString repo = findRepoRootDirectory();
    if (!repo.isEmpty()) {
        paths << QDir(repo).absoluteFilePath(QStringLiteral("cursor-stutter/latest.md"));
    }
    paths << QDir(appDataReportDirectory()).absoluteFilePath(QStringLiteral("latest.md"));
    paths.removeDuplicates();
    return paths;
}

void CursorStutterProfiler::startSampling() {
    if (!g_verbose || g_sampling) {
        return;
    }
    g_sampling = true;
    g_stopSampler.store(false, std::memory_order_relaxed);
    if (g_sessionStartUs <= 0) {
        g_sessionStartUs = steadyUs();
    }
    g_lastPeriodicFlushUs = steadyUs();
    g_samplerThread = std::thread(samplerLoop);
    qCInfo(lcCursorStutter) << "Cursor stutter sampler started; verbose=" << g_verbose;
}

void CursorStutterProfiler::flushReport(const QString& reason) {
    const QString content = buildMarkdownReport(reason);
    std::thread([content, reason]() { writeReportToPaths(content, reason); }).detach();
}

void CursorStutterProfiler::stopAndWriteReport(const QString& reason) {
    stopSamplerThread();
    const QString content = buildMarkdownReport(reason);
    writeReportToPaths(content, reason);
}

qint64 CursorStutterProfiler::monotonicUs() {
    return steadyUs();
}

void CursorStutterProfiler::recordSetCursorPos(const char* caller, int x, int y) {
    if (!g_verbose || !caller) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    bumpCounter(g_setCursorByCaller, QString::fromUtf8(caller));
    pushEventLocked("set_cursor",
                    appendForegroundContext(QStringLiteral("caller=%1 pos=(%2,%3) %4")
                                              .arg(QString::fromUtf8(caller))
                                              .arg(x)
                                              .arg(y)
                                              .arg(lockStateDetail())));
}

void CursorStutterProfiler::recordClipCursor(const char* caller) {
    if (!g_verbose || !caller) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    bumpCounter(g_clipByCaller, QString::fromUtf8(caller));
    pushEventLocked("clip_cursor",
                    appendForegroundContext(
                        QStringLiteral("caller=%1 %2").arg(QString::fromUtf8(caller)).arg(lockStateDetail())));
}

void CursorStutterProfiler::recordMouseHookSnap(int fromX, int fromY, int toX, int toY, qint64 handlerUs) {
    if (!g_verbose) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_mouseHookSnapCount;
    const int dx = toX - fromX;
    const int dy = toY - fromY;
    const int dist = static_cast<int>(std::lround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
    pushEventLocked("mouse_hook_snap",
                    appendForegroundContext(QStringLiteral("from=(%1,%2) to=(%3,%4) dist=%5px %6")
                                                .arg(fromX)
                                                .arg(fromY)
                                                .arg(toX)
                                                .arg(toY)
                                                .arg(dist)
                                                .arg(lockStateDetail())),
                    handlerUs,
                    "hook");
    if (dist >= kJumpThresholdPx) {
        noteJumpLocked(fromX, fromY, toX, toY, dist, "mouse_hook_snap", false);
    } else if (dist >= kMicroJumpThresholdPx) {
        noteJumpLocked(fromX, fromY, toX, toY, dist, "mouse_hook_snap", true);
    }
}

void CursorStutterProfiler::recordKeyboardHook(int virtualKey, bool keyDown, qint64 handlerUs, bool swallowed) {
    if (!g_verbose) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (handlerUs > 1000) {
        ++g_slowKeyboardHookCount;
    }
    QString detail = QStringLiteral("vk=0x%1 %2 swallowed=%3")
                         .arg(virtualKey, 0, 16)
                         .arg(keyDown ? QStringLiteral("down") : QStringLiteral("up"))
                         .arg(swallowed ? QStringLiteral("yes") : QStringLiteral("no"));
    if (isQwerVirtualKey(virtualKey)) {
        detail += QStringLiteral(" qwer=%1").arg(QString::fromUtf8(qwerLabel(virtualKey)));
    }
    pushEventLocked("keyboard_hook", appendForegroundContext(detail), handlerUs, "hook");
}

void CursorStutterProfiler::recordPhysicalKey(int virtualKey) {
    if (!g_verbose) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (isQwerVirtualKey(virtualKey)) {
        ++g_qwerPhysicalDown;
    }
    pushEventLocked("physical_key",
                    appendForegroundContext(isQwerVirtualKey(virtualKey)
                                                ? QString::fromUtf8(qwerLabel(virtualKey))
                                                : QStringLiteral("0x%1").arg(virtualKey, 0, 16)));
}

void CursorStutterProfiler::recordSyntheticKey(int virtualKey) {
    if (!g_verbose) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (isQwerVirtualKey(virtualKey)) {
        ++g_qwerSynthetic;
    }
    pushEventLocked("synthetic_key",
                    appendForegroundContext(isQwerVirtualKey(virtualKey)
                                                ? QString::fromUtf8(qwerLabel(virtualKey))
                                                : QStringLiteral("0x%1").arg(virtualKey, 0, 16)));
}

void CursorStutterProfiler::recordHoldFeature(const QString& featureName, const char* phase, qint64 durationUs) {
    if (!phase) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (std::strcmp(phase, "hold_start") == 0) {
        ++g_holdFeatureStartCount;
    }
    if (!g_verbose) {
        return;
    }
    pushEventLocked("hold_feature",
                    appendForegroundContext(
                        QStringLiteral("%1 phase=%2").arg(featureName, QString::fromUtf8(phase))),
                    durationUs);
}
