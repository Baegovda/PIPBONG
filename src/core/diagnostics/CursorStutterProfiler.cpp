#include "core/diagnostics/CursorStutterProfiler.h"

#include "app/MouseCenterLock.h"
#include "app/ProgramSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
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

namespace {

constexpr int kSamplerIntervalMs = 4;
constexpr int kJumpThresholdPx = 8;
constexpr int kMaxEvents = 120000;
constexpr int kMaxJumpRows = 80;

bool g_enabled = false;
std::mutex g_mutex;
bool g_sampling = false;
bool g_hasLastSample = false;
int g_lastSampleX = 0;
int g_lastSampleY = 0;
qint64 g_sessionStartUs = 0;

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
qint64 g_qwerPhysicalDown = 0;
qint64 g_qwerSynthetic = 0;
qint64 g_jumpsNearQwerMs = 0;
qint64 g_slowKeyboardHookCount = 0;

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

void pushEventLocked(const char* kind, const QString& detail, qint64 durationUs = -1, const char* threadTag = "main") {
    if (!g_enabled) {
        return;
    }
    if (static_cast<int>(g_events.size()) >= kMaxEvents) {
        g_events.erase(g_events.begin(), g_events.begin() + (g_events.size() / 10));
    }
    g_events.push_back({relUsLocked(), threadTag, kind, detail, durationUs});
}

bool envEnabled() {
#ifdef _WIN32
    wchar_t buffer[16]{};
    if (GetEnvironmentVariableW(L"PIPBONG_CURSOR_STUTTER_PROFILE", buffer, 16) > 0) {
        return buffer[0] == L'1';
    }
#endif
    return false;
}

QString repoRootDirectory() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"PIPBONG_REPO_ROOT", buffer, MAX_PATH) > 0) {
        return QDir(QString::fromWCharArray(buffer)).absolutePath();
    }
#endif
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../.."));
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

void noteJumpLocked(int fromX, int fromY, int toX, int toY, int distancePx, const char* source) {
    ++g_jumpCount;
    const QString detail = QStringLiteral("%1 from=(%2,%3) to=(%4,%5) dist=%6px %7")
                               .arg(QString::fromUtf8(source))
                               .arg(fromX)
                               .arg(fromY)
                               .arg(toX)
                               .arg(toY)
                               .arg(distancePx)
                               .arg(lockStateDetail());
    pushEventLocked("cursor_jump", detail, -1, "sampler");

    const qint64 windowStart = relUsLocked() - 100000;
    for (auto it = g_events.rbegin(); it != g_events.rend(); ++it) {
        if (it->relUs < windowStart) {
            break;
        }
        if (std::strcmp(it->kind, "physical_key") == 0 || std::strcmp(it->kind, "synthetic_key") == 0) {
            if (it->detail.contains(QLatin1String("vk=Q"))
                || it->detail.contains(QLatin1String("vk=W"))
                || it->detail.contains(QLatin1String("vk=E"))
                || it->detail.contains(QLatin1String("vk=R"))) {
                ++g_jumpsNearQwerMs;
                break;
            }
        }
    }
}

void sampleCursorPosition() {
#ifdef _WIN32
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_enabled) {
        return;
    }
    if (g_hasLastSample) {
        const int dx = pt.x - g_lastSampleX;
        const int dy = pt.y - g_lastSampleY;
        const int dist = static_cast<int>(std::lround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
        if (dist >= kJumpThresholdPx) {
            noteJumpLocked(g_lastSampleX, g_lastSampleY, pt.x, pt.y, dist, "sampler");
        }
    }
    g_lastSampleX = pt.x;
    g_lastSampleY = pt.y;
    g_hasLastSample = true;
#else
    (void)0;
#endif
}

void samplerLoop() {
    while (!g_stopSampler.load(std::memory_order_relaxed)) {
        sampleCursorPosition();
        std::this_thread::sleep_for(std::chrono::milliseconds(kSamplerIntervalMs));
    }
}

QString formatMs(qint64 us) {
    return QString::number(static_cast<double>(us) / 1000.0, 'f', 3);
}

QString buildMarkdownReport(const QString& endReason) {
    std::lock_guard<std::mutex> lock(g_mutex);

    QStringList lines;
    lines << QStringLiteral("---");
    lines << QStringLiteral("format: pipbong-cursor-stutter");
    lines << QStringLiteral("format_version: 1");
    lines << QStringLiteral("end_reason: %1").arg(endReason);
    lines << QStringLiteral("session_end: %1")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    lines << QStringLiteral("jump_threshold_px: %1").arg(kJumpThresholdPx);
    lines << QStringLiteral("sampler_interval_ms: %1").arg(kSamplerIntervalMs);
    lines << QStringLiteral("events_recorded: %1").arg(g_events.size());
    lines << QStringLiteral("---");
    lines << QString();
    lines << QStringLiteral("# Cursor stutter report");
    lines << QString();

    lines << QStringLiteral("## Auto diagnosis");
    lines << QString();
    if (g_jumpCount == 0) {
        lines << QStringLiteral("- **커서 점프**: 샘플러 기준 %1 px 이상 이동 이벤트 없음 — 재현 중 프로파일링이 켜져 있었는지 확인.")
                    .arg(kJumpThresholdPx);
    } else {
        lines << QStringLiteral("- **커서 점프**: %1회 (4 ms 샘플, ≥%2 px)")
                    .arg(g_jumpCount)
                    .arg(kJumpThresholdPx);
        if (g_jumpsNearQwerMs > 0) {
            lines << QStringLiteral(
                "- **QWER 상관**: 점프 %1회가 직전 100 ms 내 Q/W/E/R 물리·합성 키 이벤트와 겹침 — 키 입력 경로·훅·마우스 잠금 의심")
                        .arg(g_jumpsNearQwerMs);
        }
    }
    if (g_mouseHookSnapCount > 0) {
        lines << QStringLiteral(
            "- **마우스 잠금 훅**: `mouse_hook_snap` %1회 — `MouseCenterLock` WH_MOUSE_LL 이 물리 이동을 삼키고 SetCursorPos 로 되돌림")
                    .arg(g_mouseHookSnapCount);
    }
    if (!g_setCursorByCaller.empty()) {
        std::sort(g_setCursorByCaller.begin(),
                  g_setCursorByCaller.end(),
                  [](const CounterRow& a, const CounterRow& b) { return a.count > b.count; });
        lines << QStringLiteral("- **SetCursorPos 호출**: 최다 `%1` (%2회) — 클릭/이동 주입 또는 잠금 갱신")
                    .arg(g_setCursorByCaller.front().key)
                    .arg(g_setCursorByCaller.front().count);
    }
    if (g_slowKeyboardHookCount > 0) {
        lines << QStringLiteral(
            "- **느린 키보드 훅**: %1회 >1 ms — HotkeyManager LL 훅이 입력 지연을 유발할 수 있음")
                    .arg(g_slowKeyboardHookCount);
    }
    lines << QStringLiteral("- **물리 QWER**: %1회 · **합성 QWER**: %2회")
                .arg(g_qwerPhysicalDown)
                .arg(g_qwerSynthetic);
    lines << QString();
    lines << QStringLiteral("**AI**: `cursor_jump` / `set_cursor` / `mouse_hook_snap` / `keyboard_hook` 이벤트만으로 원인 분석. 사용자에게 워크플로 구성을 다시 묻지 마세요.");
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
            lines << QStringLiteral("| %1 | %2 |").arg(sorted[static_cast<size_t>(i)].key).arg(sorted[static_cast<size_t>(i)].count);
        }
        lines << QString();
    };

    emitCounterTable("SetCursorPos by caller", g_setCursorByCaller);
    emitCounterTable("ClipCursor by caller", g_clipByCaller);

    lines << QStringLiteral("## Summary counters");
    lines << QString();
    lines << QStringLiteral("| Metric | value |");
    lines << QStringLiteral("| --- | ---: |");
    lines << QStringLiteral("| cursor_jump | %1 |").arg(g_jumpCount);
    lines << QStringLiteral("| mouse_hook_snap | %1 |").arg(g_mouseHookSnapCount);
    lines << QStringLiteral("| physical_qwer | %1 |").arg(g_qwerPhysicalDown);
    lines << QStringLiteral("| synthetic_qwer | %1 |").arg(g_qwerSynthetic);
    lines << QStringLiteral("| jumps_near_qwer_100ms | %1 |").arg(g_jumpsNearQwerMs);
    lines << QStringLiteral("| slow_keyboard_hook_gt_1ms | %1 |").arg(g_slowKeyboardHookCount);
    lines << QString();

    lines << QStringLiteral("## Recent cursor jumps (newest last)");
    lines << QString();
    lines << QStringLiteral("| rel_ms | detail |");
    lines << QStringLiteral("| ---: | --- |");
    int jumpRows = 0;
    for (auto it = g_events.rbegin(); it != g_events.rend() && jumpRows < kMaxJumpRows; ++it) {
        if (std::strcmp(it->kind, "cursor_jump") != 0) {
            continue;
        }
        lines << QStringLiteral("| %1 | %2 |")
                     .arg(formatMs(it->relUs))
                     .arg(it->detail);
        ++jumpRows;
    }
    if (jumpRows == 0) {
        lines << QStringLiteral("| — | (none) |");
    }
    lines << QString();

    lines << QStringLiteral("## Event log (tsv)");
    lines << QString();
    lines << QStringLiteral("```tsv");
    lines << QStringLiteral("rel_us\tthread\tevent\tdetail\tdur_us");
    const int tail = std::min<int>(8000, static_cast<int>(g_events.size()));
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

} // namespace

bool CursorStutterProfiler::isEnabled() {
    return g_enabled;
}

void CursorStutterProfiler::reloadFromSettings() {
    g_enabled = ProgramSettings::cursorStutterProfiling() || envEnabled();
    if (g_enabled && g_sessionStartUs <= 0) {
        g_sessionStartUs = steadyUs();
    }
}

QString CursorStutterProfiler::outputDirectory() {
    return QDir(repoRootDirectory()).absoluteFilePath(QStringLiteral("cursor-stutter"));
}

QString CursorStutterProfiler::latestReportPath() {
    return QDir(outputDirectory()).absoluteFilePath(QStringLiteral("latest.md"));
}

void CursorStutterProfiler::startSampling() {
    if (!g_enabled || g_sampling) {
        return;
    }
    g_sampling = true;
    g_stopSampler.store(false, std::memory_order_relaxed);
    if (g_sessionStartUs <= 0) {
        g_sessionStartUs = steadyUs();
    }
    g_samplerThread = std::thread(samplerLoop);
}

void CursorStutterProfiler::stopAndWriteReport(const QString& reason) {
    if (!g_enabled) {
        return;
    }
    if (g_sampling) {
        g_stopSampler.store(true, std::memory_order_relaxed);
        if (g_samplerThread.joinable()) {
            g_samplerThread.join();
        }
        g_sampling = false;
    }

    const QString dirPath = outputDirectory();
    QDir().mkpath(dirPath);
    const QString path = latestReportPath();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        file.write(buildMarkdownReport(reason).toUtf8());
    }
}

qint64 CursorStutterProfiler::monotonicUs() {
    return steadyUs();
}

void CursorStutterProfiler::recordSetCursorPos(const char* caller, int x, int y) {
    if (!g_enabled || !caller) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    bumpCounter(g_setCursorByCaller, QString::fromUtf8(caller));
    pushEventLocked("set_cursor",
                    QStringLiteral("caller=%1 pos=(%2,%3) %4")
                        .arg(QString::fromUtf8(caller))
                        .arg(x)
                        .arg(y)
                        .arg(lockStateDetail()));
}

void CursorStutterProfiler::recordClipCursor(const char* caller) {
    if (!g_enabled || !caller) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    bumpCounter(g_clipByCaller, QString::fromUtf8(caller));
    pushEventLocked("clip_cursor",
                    QStringLiteral("caller=%1 %2").arg(QString::fromUtf8(caller)).arg(lockStateDetail()));
}

void CursorStutterProfiler::recordMouseHookSnap(int fromX, int fromY, int toX, int toY, qint64 handlerUs) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_mouseHookSnapCount;
    const int dx = toX - fromX;
    const int dy = toY - fromY;
    const int dist = static_cast<int>(std::lround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
    pushEventLocked("mouse_hook_snap",
                    QStringLiteral("from=(%1,%2) to=(%3,%4) dist=%5px %6")
                        .arg(fromX)
                        .arg(fromY)
                        .arg(toX)
                        .arg(toY)
                        .arg(dist)
                        .arg(lockStateDetail()),
                    handlerUs,
                    "hook");
    if (dist >= kJumpThresholdPx) {
        noteJumpLocked(fromX, fromY, toX, toY, dist, "mouse_hook_snap");
    }
}

void CursorStutterProfiler::recordKeyboardHook(int virtualKey, bool keyDown, qint64 handlerUs, bool swallowed) {
    if (!g_enabled) {
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
    pushEventLocked("keyboard_hook", detail, handlerUs, "hook");
}

void CursorStutterProfiler::recordPhysicalKey(int virtualKey) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (isQwerVirtualKey(virtualKey)) {
        ++g_qwerPhysicalDown;
    }
    pushEventLocked("physical_key",
                    QStringLiteral("vk=%1").arg(isQwerVirtualKey(virtualKey)
                                                    ? QString::fromUtf8(qwerLabel(virtualKey))
                                                    : QStringLiteral("0x%1").arg(virtualKey, 0, 16)));
}

void CursorStutterProfiler::recordSyntheticKey(int virtualKey) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (isQwerVirtualKey(virtualKey)) {
        ++g_qwerSynthetic;
    }
    pushEventLocked("synthetic_key",
                    QStringLiteral("vk=%1").arg(isQwerVirtualKey(virtualKey)
                                                    ? QString::fromUtf8(qwerLabel(virtualKey))
                                                    : QStringLiteral("0x%1").arg(virtualKey, 0, 16)));
}
