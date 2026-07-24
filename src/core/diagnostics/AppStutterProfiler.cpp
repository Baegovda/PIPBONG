#include "core/diagnostics/AppStutterProfiler.h"

#include "app/ProgramSettings.h"
#include "core/diagnostics/CpuSpikeDetector.h"
#include "core/diagnostics/DiagnosticHub.h"
#include "core/diagnostics/ProcessCpuSampler.h"
#include "core/diagnostics/SystemCpuSampler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

Q_LOGGING_CATEGORY(lcAppStutter, "pipbong.app_stutter")

namespace {

constexpr int kGuiPulseIntervalMs = 50;
constexpr int kGuiStallMildMs = 80;
constexpr int kGuiStallModerateMs = 150;
constexpr int kGuiStallSevereMs = 300;
constexpr int kGuiStallCriticalMs = 1000;
constexpr int kCpuSampleIntervalMs = 500;
constexpr int kCpuTopN = 8;
constexpr int kMaxEvents = 8000;
constexpr int kReportEventTail = 1500;

bool g_enabled = false;
std::mutex g_mutex;
qint64 g_sessionStartUs = 0;
qint64 g_lastGuiPulseMs = 0;
bool g_hasGuiPulse = false;

QTimer* g_guiPulseTimer = nullptr;
std::thread g_cpuThread;
std::atomic<bool> g_stopCpu{false};
std::atomic<bool> g_cpuRunning{false};

std::atomic<int> g_activeFeatureSessions{0};
std::atomic<bool> g_pipbongFeatureBurst{false};

qint64 g_guiPulseCount = 0;
qint64 g_guiStallMild = 0;
qint64 g_guiStallModerate = 0;
qint64 g_guiStallSevere = 0;
qint64 g_guiStallCritical = 0;
qint64 g_maxGuiStallMs = 0;
qint64 g_cpuSampleCount = 0;
qint64 g_cpuSpikeCount = 0;
double g_peakSystemCpu = 0.0;
double g_peakPipbongCpu = 0.0;
qint64 g_foregroundChangeCount = 0;
quintptr g_lastForegroundHwnd = static_cast<quintptr>(-1);
QString g_cachedForegroundDetail = QStringLiteral("fg=(unknown)");

struct EventRow {
    qint64 relUs = 0;
    const char* threadTag = "main";
    const char* kind = "";
    QString detail;
    qint64 durationUs = -1;
};

std::vector<EventRow> g_events;

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

bool envVarEnabled(const wchar_t* name) {
#ifdef _WIN32
    wchar_t buffer[16]{};
    return GetEnvironmentVariableW(name, buffer, 16) > 0 && buffer[0] == L'1';
#else
    Q_UNUSED(name);
    return false;
#endif
}

bool envEnabled() {
    return envVarEnabled(L"PIPBONG_APP_STUTTER_PROFILE")
           || envVarEnabled(L"PIPBONG_APP_SPIKE_PROFILE")
           || envVarEnabled(L"PIPBONG_CURSOR_STUTTER_PROFILE");
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
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(base).filePath(QStringLiteral("PIPBONG/PIPBONG/app-stutter"));
}

QString appendSessionContext(const QString& detail) {
    QString line = detail;
    const int sessions = g_activeFeatureSessions.load(std::memory_order_relaxed);
    line += QStringLiteral(" sessions=%1 burst=%2")
                .arg(sessions)
                .arg(g_pipbongFeatureBurst.load(std::memory_order_relaxed) ? QStringLiteral("yes")
                                                                           : QStringLiteral("no"));
    line += QStringLiteral(" ") + g_cachedForegroundDetail;
    return line;
}

void pushEventLocked(const char* kind,
                     const QString& detail,
                     qint64 durationUs = -1,
                     const char* threadTag = "main") {
    if (!g_enabled) {
        return;
    }
    if (static_cast<int>(g_events.size()) >= kMaxEvents) {
        g_events.erase(g_events.begin(), g_events.begin() + (g_events.size() / 10));
    }
    g_events.push_back({relUsLocked(), threadTag, kind, detail, durationUs});
}

#ifdef _WIN32
void refreshForegroundCacheLocked(HWND fg) {
    if (!fg || !IsWindow(fg)) {
        g_cachedForegroundDetail = QStringLiteral("fg=(none)");
        return;
    }
    wchar_t title[512]{};
    GetWindowTextW(fg, title, 512);
    const QString titleText =
        title[0] ? QString::fromWCharArray(title) : QStringLiteral("(empty title)");

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    QString exeName = QStringLiteral("?");
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        wchar_t exePath[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, exePath, &size)) {
            const QString path = QString::fromWCharArray(exePath);
            const int slash = path.lastIndexOf(QLatin1Char('\\'));
            exeName = slash >= 0 ? path.mid(slash + 1) : path;
        }
        CloseHandle(process);
    }

    g_cachedForegroundDetail =
        QStringLiteral("fg=\"%1\" hwnd=0x%2 exe=%3")
            .arg(titleText)
            .arg(reinterpret_cast<quintptr>(fg), 0, 16)
            .arg(exeName);
}

void pollForegroundWindowChange() {
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
    pushEventLocked("foreground_focus", g_cachedForegroundDetail, -1, "cpu");
}
#endif

void recordGuiStallLocked(qint64 gapMs) {
    if (gapMs > g_maxGuiStallMs) {
        g_maxGuiStallMs = gapMs;
    }

    const char* severity = "gui_stall_mild";
    if (gapMs >= kGuiStallCriticalMs) {
        ++g_guiStallCritical;
        severity = "gui_stall_critical";
    } else if (gapMs >= kGuiStallSevereMs) {
        ++g_guiStallSevere;
        severity = "gui_stall_severe";
    } else if (gapMs >= kGuiStallModerateMs) {
        ++g_guiStallModerate;
        severity = "gui_stall_moderate";
    } else if (gapMs >= kGuiStallMildMs) {
        ++g_guiStallMild;
    } else {
        return;
    }

    QString detail = appendSessionContext(QStringLiteral("gap_ms=%1").arg(gapMs));
    const auto crumbs = DiagnosticHub::snapshotBreadcrumbs(5);
    if (!crumbs.empty()) {
        QStringList parts;
        for (const auto& crumb : crumbs) {
            parts << QStringLiteral("[%1] %2").arg(crumb.category, crumb.message);
        }
        detail += QStringLiteral(" crumbs=") + parts.join(QStringLiteral(" | "));
    }
    pushEventLocked(severity, detail, gapMs * 1000);
}

void cpuMonitorLoop() {
    SystemCpuSampler systemSampler;
    ProcessCpuSampler processSampler(systemSampler.logicalProcessorCount());
    CpuSpikeDetector spikeDetector;
    CpuSpikeDetectorConfig config;
    config.systemThresholdPercent = 75.0;
    config.processThresholdPercent = 35.0;
    config.deltaMarginPercent = 20.0;
    config.cooldownMs = 3000;
    spikeDetector.setConfig(config);

#ifdef _WIN32
    const quint32 selfPid = static_cast<quint32>(GetCurrentProcessId());
#else
    const quint32 selfPid = 0;
#endif

    while (!g_stopCpu.load(std::memory_order_relaxed)) {
        pollForegroundWindowChange();

        CpuSampleSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTime();
        const double systemCpu = systemSampler.sampleCpuPercent();
        snapshot.systemReady = systemCpu >= 0.0;
        snapshot.systemCpuPercent = snapshot.systemReady ? systemCpu : 0.0;
        snapshot.topProcesses =
            processSampler.sampleTopProcesses(kCpuTopN, [&]() { return g_stopCpu.load(); });

        if (!g_stopCpu.load()) {
            double pipbongCpu = 0.0;
            for (const ProcessCpuEntry& entry : snapshot.topProcesses) {
                if (entry.pid == selfPid) {
                    pipbongCpu = entry.cpuPercent;
                    break;
                }
            }

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                ++g_cpuSampleCount;
                if (snapshot.systemReady) {
                    g_peakSystemCpu = std::max(g_peakSystemCpu, snapshot.systemCpuPercent);
                }
                g_peakPipbongCpu = std::max(g_peakPipbongCpu, pipbongCpu);
            }

            if (snapshot.systemReady) {
                const bool featureBurst = g_pipbongFeatureBurst.load(std::memory_order_relaxed);
                if (const auto spike = spikeDetector.evaluate(snapshot, featureBurst)) {
                    QString topLine;
                    for (int i = 0; i < std::min(3, static_cast<int>(spike->topProcesses.size())); ++i) {
                        const ProcessCpuEntry& e = spike->topProcesses[i];
                        if (!topLine.isEmpty()) {
                            topLine += QStringLiteral(", ");
                        }
                        topLine += QStringLiteral("%1=%2%")
                                        .arg(e.name)
                                        .arg(e.cpuPercent, 0, 'f', 1);
                    }
                    const QString detail = appendSessionContext(
                        QStringLiteral("%1 sys=%2% top=[%3]")
                            .arg(spike->triggerDetail)
                            .arg(spike->systemCpuPercent, 0, 'f', 1)
                            .arg(topLine));
                    std::lock_guard<std::mutex> lock(g_mutex);
                    ++g_cpuSpikeCount;
                    pushEventLocked("cpu_spike", detail, -1, "cpu");
                }
            }
        }

        for (int slept = 0; slept < kCpuSampleIntervalMs && !g_stopCpu.load(); slept += 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    g_cpuRunning.store(false, std::memory_order_relaxed);
}

void startGuiPulseTimer() {
    if (!QCoreApplication::instance()) {
        return;
    }
    if (!g_guiPulseTimer) {
        g_guiPulseTimer = new QTimer(QCoreApplication::instance());
        g_guiPulseTimer->setInterval(kGuiPulseIntervalMs);
        QObject::connect(g_guiPulseTimer, &QTimer::timeout, g_guiPulseTimer, []() {
            AppStutterProfiler::noteGuiPulse();
        });
    }
    if (!g_guiPulseTimer->isActive()) {
        g_guiPulseTimer->start();
    }
}

void stopGuiPulseTimer() {
    if (g_guiPulseTimer) {
        g_guiPulseTimer->stop();
    }
}

void startCpuThread() {
    if (g_cpuRunning.exchange(true)) {
        return;
    }
    g_stopCpu.store(false, std::memory_order_relaxed);
    g_cpuThread = std::thread(cpuMonitorLoop);
}

void stopCpuThread() {
    g_stopCpu.store(true, std::memory_order_relaxed);
    if (g_cpuThread.joinable()) {
        g_cpuThread.join();
    }
    g_cpuRunning.store(false, std::memory_order_relaxed);
}

QString buildMarkdownReport(const QString& reason) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const qint64 sessionUs = g_sessionStartUs > 0 ? relUsLocked() : 0;
    const double sessionSec = sessionUs / 1'000'000.0;

    QStringList lines;
    lines << QStringLiteral("---");
    lines << QStringLiteral("format: pipbong-app-stutter");
    lines << QStringLiteral("format_version: 1");
    lines << QStringLiteral("end_reason: %1").arg(reason);
    lines << QStringLiteral("session_end: %1")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    lines << QStringLiteral("profiling_enabled: %1").arg(g_enabled ? QStringLiteral("yes")
                                                                   : QStringLiteral("no"));
    lines << QStringLiteral("session_seconds: %1").arg(sessionSec, 0, 'f', 1);
    lines << QStringLiteral("gui_pulse_interval_ms: %1").arg(kGuiPulseIntervalMs);
    lines << QStringLiteral("cpu_sample_interval_ms: %1").arg(kCpuSampleIntervalMs);
    lines << QStringLiteral("events_recorded: %1").arg(g_events.size());
    lines << QStringLiteral("foreground_at_end: %1").arg(g_cachedForegroundDetail);
    lines << QStringLiteral("foreground_changes: %1").arg(g_foregroundChangeCount);
    const QStringList paths = AppStutterProfiler::allReportPaths();
    lines << QStringLiteral("report_paths: %1").arg(paths.join(QStringLiteral("; ")));
    lines << QStringLiteral("---");
    lines << QString();
    lines << QStringLiteral("# App stutter report");
    lines << QString();
    lines << QStringLiteral("## Auto diagnosis");
    lines << QString();
    lines << QStringLiteral("- **종료 시 포커스 창**: %1").arg(g_cachedForegroundDetail);
    lines << QString();

    const qint64 totalGuiStalls =
        g_guiStallMild + g_guiStallModerate + g_guiStallSevere + g_guiStallCritical;
    if (g_guiStallCritical > 0 || g_maxGuiStallMs >= kGuiStallCriticalMs) {
        lines << QStringLiteral(
            "- **PIPBONG UI 정지 의심**: 최대 GUI 간격 **%1 ms** (치명적 ≥%2 ms: %3회)")
                     .arg(g_maxGuiStallMs)
                     .arg(kGuiStallCriticalMs)
                     .arg(g_guiStallCritical);
    } else if (g_guiStallSevere > 0 || g_maxGuiStallMs >= kGuiStallSevereMs) {
        lines << QStringLiteral(
            "- **PIPBONG UI 버벅임**: 최대 GUI 간격 **%1 ms** (심각 ≥%2 ms: %3회)")
                     .arg(g_maxGuiStallMs)
                     .arg(kGuiStallSevereMs)
                     .arg(g_guiStallSevere);
    } else if (totalGuiStalls > 0) {
        lines << QStringLiteral("- **가벼운 UI 지연**: GUI 펄스 간격 초과 %1회 (최대 %2 ms)")
                     .arg(totalGuiStalls)
                     .arg(g_maxGuiStallMs);
    } else {
        lines << QStringLiteral("- **GUI 스톨**: 기록된 UI 지연 없음 (≥%1 ms)")
                     .arg(kGuiStallMildMs);
    }

    if (g_cpuSpikeCount > 0) {
        lines << QStringLiteral(
            "- **CPU 스파이크**: %1회 (시스템 최대 %2%%, PIPBONG 최대 %3%%)")
                     .arg(g_cpuSpikeCount)
                     .arg(g_peakSystemCpu, 0, 'f', 1)
                     .arg(g_peakPipbongCpu, 0, 'f', 1);
    } else {
        lines << QStringLiteral("- **CPU 스파이크**: 임계 초과 없음 (시스템 최대 %1%%, PIPBONG 최대 %2%%)")
                     .arg(g_peakSystemCpu, 0, 'f', 1)
                     .arg(g_peakPipbongCpu, 0, 'f', 1);
    }

    lines << QString();
    lines << QStringLiteral(
        "**AI**: `gui_stall_*` / `cpu_spike` / `foreground_focus` 로 원인 분석. 워크플로 구성을 사용자에게 다시 묻지 마세요.");
    lines << QString();
    lines << QStringLiteral("## Summary counters");
    lines << QString();
    lines << QStringLiteral("| Metric | value |");
    lines << QStringLiteral("| --- | ---: |");
    lines << QStringLiteral("| session_seconds | %1 |").arg(sessionSec, 0, 'f', 1);
    lines << QStringLiteral("| gui_pulse_count | %1 |").arg(g_guiPulseCount);
    lines << QStringLiteral("| gui_stall_mild (>=%1 ms) | %2 |")
                 .arg(kGuiStallMildMs)
                 .arg(g_guiStallMild);
    lines << QStringLiteral("| gui_stall_moderate (>=%1 ms) | %2 |")
                 .arg(kGuiStallModerateMs)
                 .arg(g_guiStallModerate);
    lines << QStringLiteral("| gui_stall_severe (>=%1 ms) | %2 |")
                 .arg(kGuiStallSevereMs)
                 .arg(g_guiStallSevere);
    lines << QStringLiteral("| gui_stall_critical (>=%1 ms) | %2 |")
                 .arg(kGuiStallCriticalMs)
                 .arg(g_guiStallCritical);
    lines << QStringLiteral("| max_gui_stall_ms | %1 |").arg(g_maxGuiStallMs);
    lines << QStringLiteral("| cpu_samples | %1 |").arg(g_cpuSampleCount);
    lines << QStringLiteral("| cpu_spike | %1 |").arg(g_cpuSpikeCount);
    lines << QStringLiteral("| peak_system_cpu_percent | %1 |").arg(g_peakSystemCpu, 0, 'f', 1);
    lines << QStringLiteral("| peak_pipbong_cpu_percent | %1 |").arg(g_peakPipbongCpu, 0, 'f', 1);
    lines << QStringLiteral("| foreground_focus_changes | %1 |").arg(g_foregroundChangeCount);
    lines << QString();
    lines << QStringLiteral("## Event log (tsv)");
    lines << QString();
    lines << QStringLiteral("```tsv");
    lines << QStringLiteral("rel_us\tthread\tevent\tdetail\tdur_us");
    const int tail = std::min<int>(kReportEventTail, static_cast<int>(g_events.size()));
    for (int i = static_cast<int>(g_events.size()) - tail; i < static_cast<int>(g_events.size());
         ++i) {
        const EventRow& row = g_events[static_cast<size_t>(i)];
        lines << QStringLiteral("%1\t%2\t%3\t%4\t%5")
                     .arg(row.relUs)
                     .arg(QString::fromUtf8(row.threadTag))
                     .arg(QString::fromUtf8(row.kind))
                     .arg(row.detail)
                     .arg(row.durationUs);
    }
    lines << QStringLiteral("```");
    return lines.join(QLatin1Char('\n'));
}

bool writeReportFiles(const QString& reason) {
    const QString content = buildMarkdownReport(reason);
    bool anyWritten = false;
    for (const QString& path : AppStutterProfiler::allReportPaths()) {
        const QFileInfo info(path);
        QDir().mkpath(info.absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            qCWarning(lcAppStutter) << "Failed to write app stutter report:" << path
                                    << file.errorString();
            continue;
        }
        file.write(content.toUtf8());
        anyWritten = true;
        qCInfo(lcAppStutter) << "Wrote app stutter report (" << reason << "):" << path;
    }
    if (!anyWritten) {
        qCWarning(lcAppStutter) << "No app stutter report path writable for reason:" << reason;
    }
    return anyWritten;
}

} // namespace

bool AppStutterProfiler::isEnabled() {
    return g_enabled;
}

void AppStutterProfiler::reloadFromSettings() {
    const bool want = ProgramSettings::appStutterProfiling() || envEnabled();
    if (want == g_enabled) {
        return;
    }

    if (g_enabled) {
        stopGuiPulseTimer();
        stopCpuThread();
        flushReport(QStringLiteral("profiling_disabled"));
    }

    g_enabled = want;
    if (!g_enabled) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_events.clear();
        g_guiPulseCount = 0;
        g_guiStallMild = 0;
        g_guiStallModerate = 0;
        g_guiStallSevere = 0;
        g_guiStallCritical = 0;
        g_maxGuiStallMs = 0;
        g_cpuSampleCount = 0;
        g_cpuSpikeCount = 0;
        g_peakSystemCpu = 0.0;
        g_peakPipbongCpu = 0.0;
        g_foregroundChangeCount = 0;
        g_sessionStartUs = steadyUs();
        g_lastGuiPulseMs = 0;
        g_hasGuiPulse = false;
    }
    startGuiPulseTimer();
    startCpuThread();
}

QString AppStutterProfiler::outputDirectory() {
    const QString repo = findRepoRootDirectory();
    if (!repo.isEmpty()) {
        return QDir(repo).filePath(QStringLiteral("app-stutter"));
    }
    return appDataOutputDirectory();
}

QString AppStutterProfiler::latestReportPath() {
    return QDir(outputDirectory()).filePath(QStringLiteral("latest.md"));
}

QStringList AppStutterProfiler::allReportPaths() {
    QStringList paths;
    const QString repo = findRepoRootDirectory();
    if (!repo.isEmpty()) {
        paths << QDir(repo).filePath(QStringLiteral("app-stutter/latest.md"));
    }
    paths << QDir(appDataOutputDirectory()).filePath(QStringLiteral("latest.md"));
    paths.removeDuplicates();
    return paths;
}

void AppStutterProfiler::flushReport(const QString& reason) {
    if (!g_enabled && g_events.empty() && g_sessionStartUs <= 0) {
        return;
    }
    writeReportFiles(reason);
}

void AppStutterProfiler::stopAndWriteReport(const QString& reason) {
    if (g_enabled) {
        stopGuiPulseTimer();
        stopCpuThread();
        g_enabled = false;
    }
    writeReportFiles(reason);
}

void AppStutterProfiler::noteGuiPulse() {
    if (!g_enabled) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_guiPulseCount;
    if (g_hasGuiPulse) {
        const qint64 gapMs = nowMs - g_lastGuiPulseMs;
        recordGuiStallLocked(gapMs);
    }
    g_lastGuiPulseMs = nowMs;
    g_hasGuiPulse = true;
}

void AppStutterProfiler::setActiveFeatureSessionCount(int count) {
    g_activeFeatureSessions.store(std::max(0, count), std::memory_order_relaxed);
}

void AppStutterProfiler::setPipbongFeatureBurstActive(bool active) {
    g_pipbongFeatureBurst.store(active, std::memory_order_relaxed);
}
