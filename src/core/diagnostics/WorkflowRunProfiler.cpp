#include "core/diagnostics/WorkflowRunProfiler.h"

#include "core/diagnostics/WorkflowProfileSnapshot.h"
#include "PipbongVersion.h"
#include "app/ProgramSettings.h"
#include "model/Feature.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QStringConverter>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int kMaxEventLines = 600000;
constexpr qint64 kSpikeLoopGapUs = 16000;
constexpr qint64 kSpikeLoopDurationUs = 32000;
constexpr qint64 kSpikeUiFlushUs = 8000;
constexpr qint64 kSpikeClickUs = 12000;

struct ProfileEvent {
    qint64 relUs = 0;
    QString threadTag;
    QString eventName;
    QString detail;
    qint64 durationUs = -1;
};

struct LoopStats {
    int iteration = 0;
    qint64 durationUs = 0;
    qint64 relEndUs = 0;
};

struct LoopGapSample {
    qint64 gapUs = 0;
    qint64 relUs = 0;
    int afterLoopIteration = 0;
};

struct BlockRunAggregate {
    int oneBasedIndex = 0;
    QString type;
    QString config;
    int execCount = 0;
    int successCount = 0;
    int failCount = 0;
    qint64 totalDurationUs = 0;
    qint64 maxDurationUs = 0;
};

struct SessionState {
    bool appActive = false;
    bool featureSessionActive = false;
    qint64 originUs = 0;
    QString featureId;
    QString featureName;
    QString runMode;
    QString profileName;
    qint64 sessionStartEpochMs = 0;

    std::vector<ProfileEvent> events;
    std::vector<LoopStats> loops;
    std::vector<LoopGapSample> loopGapSamples;
    std::vector<qint64> uiFlushDurationsUs;
    std::vector<qint64> clickDurationsUs;

    qint64 lastLoopEndRelUs = -1;
    int lastLoopIteration = 0;

    int loopBeginCount = 0;
    int loopEndCount = 0;
    int blockEndCount = 0;
    int clickCount = 0;
    int uiFlushCount = 0;
    int droppedEvents = 0;

    QString startSource;
    int workflowBlockCount = 0;
    QStringList featureSettingsLines;
    std::vector<WorkflowBlockSnapshotRow> blockSnapshots;
    std::vector<BlockRunAggregate> blockAggregates;
    qint64 featureSessionBeginRelUs = 0;
};

std::mutex g_mutex;
SessionState g_trace;
bool g_enabled = false;
unsigned long g_mainThreadId = 0;

const char* threadTagForCurrentThread() {
#ifdef _WIN32
    if (g_mainThreadId == 0) {
        g_mainThreadId = GetCurrentThreadId();
    }
    return GetCurrentThreadId() == g_mainThreadId ? "ui" : "worker";
#else
    return "thread";
#endif
}

qint64 queryPerformanceCounterUs() {
#ifdef _WIN32
    static LARGE_INTEGER frequency = {};
    static bool initialized = false;
    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = true;
    }
    LARGE_INTEGER counter = {};
    QueryPerformanceCounter(&counter);
    if (frequency.QuadPart <= 0) {
        return QDateTime::currentMSecsSinceEpoch() * 1000;
    }
    return static_cast<qint64>((counter.QuadPart * 1000000LL) / frequency.QuadPart);
#else
    return QDateTime::currentMSecsSinceEpoch() * 1000;
#endif
}

QString sanitizeFileToken(QString value) {
    value = value.trimmed();
    if (value.isEmpty()) {
        return QStringLiteral("feature");
    }
    for (QChar& ch : value) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }
    return value.left(48);
}

QString findRepositoryRoot() {
    const QByteArray override = qgetenv("PIPBONG_REPO_ROOT");
    if (!override.isEmpty()) {
        return QDir::fromNativeSeparators(QString::fromUtf8(override));
    }

    const auto isRepoRoot = [](const QString& dirPath) {
        const QFileInfo cmakeFile(QDir(dirPath).filePath(QStringLiteral("CMakeLists.txt")));
        if (!cmakeFile.exists() || !cmakeFile.isFile()) {
            return false;
        }
        QFile cmake(cmakeFile.absoluteFilePath());
        if (!cmake.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
        return QString::fromUtf8(cmake.read(4096)).contains(QStringLiteral("project(PIPBONG"));
    };

    QStringList seeds;
    if (QCoreApplication::instance() != nullptr) {
        const QString appDir = QCoreApplication::applicationDirPath();
        if (!appDir.isEmpty()) {
            seeds.append(QDir(appDir).absolutePath());
        }
    }
    const QString cwd = QDir::currentPath();
    if (!cwd.isEmpty() && !seeds.contains(cwd)) {
        seeds.append(cwd);
    }

    for (const QString& seed : seeds) {
        QDir dir(seed);
        for (int depth = 0; depth < 8; ++depth) {
            const QString absolute = dir.absolutePath();
            if (isRepoRoot(absolute)) {
                return absolute;
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    return seeds.isEmpty() ? QStringLiteral(".") : seeds.front();
}

QString percentileLabel(const std::vector<qint64>& values, double percentile) {
    if (values.empty()) {
        return QStringLiteral("n/a");
    }
    std::vector<qint64> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const double rank = (percentile / 100.0) * static_cast<double>(sorted.size() - 1);
    const int lower = static_cast<int>(std::floor(rank));
    const int upper = static_cast<int>(std::ceil(rank));
    const qint64 sample =
        lower == upper ? sorted[static_cast<size_t>(lower)]
                       : static_cast<qint64>(
                             sorted[static_cast<size_t>(lower)]
                             + (sorted[static_cast<size_t>(upper)] - sorted[static_cast<size_t>(lower)])
                                   * (rank - static_cast<double>(lower)));
    return QString::number(sample / 1000.0, 'f', 3);
}

qint64 maxValue(const std::vector<qint64>& values) {
    if (values.empty()) {
        return 0;
    }
    return *std::max_element(values.begin(), values.end());
}

int countAbove(const std::vector<qint64>& values, qint64 thresholdUs) {
    int count = 0;
    for (qint64 value : values) {
        if (value > thresholdUs) {
            ++count;
        }
    }
    return count;
}

QString escapeMd(QString value) {
    value.replace(QLatin1Char('|'), QLatin1Char('/'));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value;
}

QString formatMs(qint64 micros, int decimals = 3) {
    return QString::number(micros / 1000.0, 'f', decimals);
}

void appendEventSeriesTable(QStringList& lines, const SessionState& session) {
    std::unordered_map<QString, std::vector<qint64>> durationsByEvent;
    std::unordered_map<QString, int> countsByEvent;
    for (const ProfileEvent& event : session.events) {
        ++countsByEvent[event.eventName];
        if (event.durationUs >= 0) {
            durationsByEvent[event.eventName].push_back(event.durationUs);
        }
    }

    struct EventSeriesRow {
        QString name;
        int count = 0;
        QString p50;
        QString p95;
        QString maxMs;
    };
    std::vector<EventSeriesRow> rows;
    rows.reserve(countsByEvent.size());
    for (const auto& entry : countsByEvent) {
        EventSeriesRow row;
        row.name = entry.first;
        row.count = entry.second;
        const auto durIt = durationsByEvent.find(entry.first);
        if (durIt != durationsByEvent.end() && !durIt->second.empty()) {
            row.p50 = percentileLabel(durIt->second, 50.0);
            row.p95 = percentileLabel(durIt->second, 95.0);
            row.maxMs = formatMs(maxValue(durIt->second));
        } else {
            row.p50 = QStringLiteral("—");
            row.p95 = QStringLiteral("—");
            row.maxMs = QStringLiteral("—");
        }
        rows.push_back(std::move(row));
    }
    std::sort(rows.begin(), rows.end(), [](const EventSeriesRow& a, const EventSeriesRow& b) {
        return a.count > b.count;
    });

    lines << QString();
    lines << QStringLiteral("## Event series (milliseconds)");
    lines << QString();
    lines << QStringLiteral("| event | count | p50 | p95 | max |");
    lines << QStringLiteral("| --- | ---: | ---: | ---: | ---: |");
    const int seriesLimit = std::min<int>(40, static_cast<int>(rows.size()));
    if (seriesLimit == 0) {
        lines << QStringLiteral("| — | — | — | — | — |");
        return;
    }
    for (int i = 0; i < seriesLimit; ++i) {
        const EventSeriesRow& row = rows[static_cast<size_t>(i)];
        lines << QStringLiteral("| %1 | %2 | %3 | %4 | %5 |")
                     .arg(escapeMd(row.name))
                     .arg(row.count)
                     .arg(row.p50)
                     .arg(row.p95)
                     .arg(row.maxMs);
    }
}

void resetFeatureSessionMetrics(SessionState& session);
std::vector<ProfileEventLite> featureSessionEvents(const SessionState& session);
std::vector<BlockRunMeasuredRow> measuredRowsFromAggregates(const SessionState& session);

QString buildMarkdownReport(const SessionState& session, const QString& endReason) {
    const QString sessionStart =
        QDateTime::fromMSecsSinceEpoch(session.sessionStartEpochMs).toString(Qt::ISODateWithMs);
    const QString sessionEnd = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);

    std::vector<qint64> loopDurations;
    loopDurations.reserve(session.loops.size());
    for (const LoopStats& loop : session.loops) {
        loopDurations.push_back(loop.durationUs);
    }

    std::vector<qint64> loopGapValuesUs;
    loopGapValuesUs.reserve(session.loopGapSamples.size());
    for (const LoopGapSample& sample : session.loopGapSamples) {
        loopGapValuesUs.push_back(sample.gapUs);
    }

    QStringList lines;
    lines << QStringLiteral("---");
    lines << QStringLiteral("format: pipbong-app-profile");
    lines << QStringLiteral("format_version: 4");
    lines << QStringLiteral("app_version: %1").arg(QStringLiteral(PIPBONG_VERSION));
    lines << QStringLiteral("git_sha: %1").arg(QStringLiteral(PIPBONG_GIT_SHA));
    lines << QStringLiteral("session_start: %1").arg(sessionStart);
    lines << QStringLiteral("session_end: %1").arg(sessionEnd);
    lines << QStringLiteral("end_reason: %1").arg(escapeMd(endReason));
    lines << QStringLiteral("feature_id: %1").arg(escapeMd(session.featureId));
    lines << QStringLiteral("feature_name: %1").arg(escapeMd(session.featureName));
    lines << QStringLiteral("run_mode: %1").arg(escapeMd(session.runMode));
    lines << QStringLiteral("profile_name: %1").arg(escapeMd(session.profileName));
    lines << QStringLiteral("workflow_block_count: %1").arg(session.workflowBlockCount);
    if (!session.startSource.isEmpty()) {
        lines << QStringLiteral("session_start_source: %1").arg(escapeMd(session.startSource));
    }
    lines << QStringLiteral("events_recorded: %1").arg(session.events.size());
    lines << QStringLiteral("events_dropped: %1").arg(session.droppedEvents);
    lines << QStringLiteral("---");
    lines << QString();
    lines << QStringLiteral("# App profile");
    lines << QString();
    lines << QStringLiteral("## Session");
    lines << QString();
    lines << QStringLiteral("| Field | Value |");
    lines << QStringLiteral("| --- | --- |");
    lines << QStringLiteral("| Profile | %1 |").arg(escapeMd(session.profileName));
    if (session.featureSessionActive || !session.featureName.isEmpty()) {
        lines << QStringLiteral("| Feature | %1 (`%2`) |")
                     .arg(escapeMd(session.featureName), escapeMd(session.featureId));
        lines << QStringLiteral("| Run mode | %1 |").arg(escapeMd(session.runMode));
        if (!session.startSource.isEmpty()) {
            lines << QStringLiteral("| Start source | %1 |").arg(escapeMd(session.startSource));
        }
        lines << QStringLiteral("| Workflow blocks | %1 |").arg(session.workflowBlockCount);
    } else {
        lines << QStringLiteral("| Feature | (no feature session) |");
    }
    lines << QStringLiteral("| Started | %1 |").arg(sessionStart);
    lines << QStringLiteral("| Ended | %1 |").arg(sessionEnd);
    lines << QStringLiteral("| End reason | %1 |").arg(escapeMd(endReason));
    lines << QStringLiteral("| Events | %1 recorded, %2 dropped |")
                 .arg(session.events.size())
                 .arg(session.droppedEvents);
    lines << QStringLiteral("| Counters | loops %1/%2, blocks %3, clicks %4, ui_flush %5 |")
                 .arg(session.loopBeginCount)
                 .arg(session.loopEndCount)
                 .arg(session.blockEndCount)
                 .arg(session.clickCount)
                 .arg(session.uiFlushCount);
    lines << QString();
    lines << QStringLiteral("## Metrics (milliseconds)");
    lines << QString();
    lines << QStringLiteral("| Series | count | p50 | p95 | p99 | max |");
    lines << QStringLiteral("| --- | ---: | ---: | ---: | ---: | ---: |");
    lines << QStringLiteral("| loop_duration | %1 | %2 | %3 | %4 | %5 |")
                 .arg(loopDurations.size())
                 .arg(percentileLabel(loopDurations, 50.0))
                 .arg(percentileLabel(loopDurations, 95.0))
                 .arg(percentileLabel(loopDurations, 99.0))
                 .arg(formatMs(maxValue(loopDurations)));
    lines << QStringLiteral("| loop_gap | %1 | %2 | %3 | %4 | %5 |")
                 .arg(loopGapValuesUs.size())
                 .arg(percentileLabel(loopGapValuesUs, 50.0))
                 .arg(percentileLabel(loopGapValuesUs, 95.0))
                 .arg(percentileLabel(loopGapValuesUs, 99.0))
                 .arg(formatMs(maxValue(loopGapValuesUs)));
    lines << QStringLiteral("| ui_fast_repeat_flush | %1 | — | %2 | — | %3 |")
                 .arg(session.uiFlushCount)
                 .arg(percentileLabel(session.uiFlushDurationsUs, 95.0))
                 .arg(formatMs(maxValue(session.uiFlushDurationsUs)));
    lines << QStringLiteral("| mouse_click | %1 | — | %2 | — | %3 |")
                 .arg(session.clickCount)
                 .arg(percentileLabel(session.clickDurationsUs, 95.0))
                 .arg(formatMs(maxValue(session.clickDurationsUs)));
    lines << QString();
    lines << QStringLiteral("## Spike counts");
    lines << QString();
    lines << QStringLiteral("| Kind | threshold | count |");
    lines << QStringLiteral("| --- | --- | ---: |");
    lines << QStringLiteral("| loop_gap | >16 ms | %1 |").arg(countAbove(loopGapValuesUs, 16000));
    lines << QStringLiteral("| loop_gap | >32 ms | %1 |").arg(countAbove(loopGapValuesUs, 32000));
    lines << QStringLiteral("| loop_gap | >50 ms | %1 |").arg(countAbove(loopGapValuesUs, 50000));
    lines << QStringLiteral("| loop_duration | >32 ms | %1 |")
                 .arg(countAbove(loopDurations, kSpikeLoopDurationUs));
    lines << QStringLiteral("| ui_flush | >8 ms | %1 |")
                 .arg(countAbove(session.uiFlushDurationsUs, kSpikeUiFlushUs));
    lines << QStringLiteral("| mouse_click | >12 ms | %1 |")
                 .arg(countAbove(session.clickDurationsUs, kSpikeClickUs));
    lines << QString();

    const std::vector<BlockRunMeasuredRow> measuredRows = measuredRowsFromAggregates(session);
    lines += buildFeatureSettingsMarkdown(session.featureSettingsLines);
    lines << QString();
    lines += buildWorkflowBlocksMarkdown(session.blockSnapshots);
    lines << QString();
    lines += buildBlockExecutionMarkdown(measuredRows);
    lines << QString();

    ProfileDiagnosisInput diagnosisInput;
    diagnosisInput.featureName = session.featureName;
    diagnosisInput.runMode = session.runMode;
    diagnosisInput.endReason = endReason;
    diagnosisInput.startSource = session.startSource;
    diagnosisInput.workflowBlockCount = session.workflowBlockCount;
    diagnosisInput.featureSettings = session.featureSettingsLines;
    diagnosisInput.blockSnapshots = session.blockSnapshots;
    diagnosisInput.blockMeasured = measuredRows;
    diagnosisInput.sessionEvents = featureSessionEvents(session);
    diagnosisInput.loopBeginCount = session.loopBeginCount;
    diagnosisInput.loopEndCount = session.loopEndCount;
    diagnosisInput.clickCount = session.clickCount;
    diagnosisInput.maxLoopGapUs = maxValue(loopGapValuesUs);
    lines += buildAutoDiagnosis(diagnosisInput);
    lines << QString();
    lines << QStringLiteral("## Top loop_gap spikes");
    lines << QString();
    lines << QStringLiteral("| rel (ms) | gap (ms) | after_loop |");
    lines << QStringLiteral("| ---: | ---: | ---: |");
    std::vector<LoopGapSample> gapSamples = session.loopGapSamples;
    std::sort(gapSamples.begin(),
              gapSamples.end(),
              [](const LoopGapSample& a, const LoopGapSample& b) { return a.gapUs > b.gapUs; });
    const int gapLimit = std::min<int>(20, static_cast<int>(gapSamples.size()));
    if (gapLimit == 0) {
        lines << QStringLiteral("| — | — | — |");
    } else {
        for (int i = 0; i < gapLimit; ++i) {
            const LoopGapSample& sample = gapSamples[static_cast<size_t>(i)];
            lines << QStringLiteral("| %1 | %2 | %3 |")
                         .arg(formatMs(sample.relUs))
                         .arg(formatMs(sample.gapUs))
                         .arg(sample.afterLoopIteration);
        }
    }
    lines << QString();
    lines << QStringLiteral("## Top ui_flush spikes");
    lines << QString();
    lines << QStringLiteral("| rel (ms) | dur (ms) | detail |");
    lines << QStringLiteral("| ---: | ---: | --- |");
    std::vector<const ProfileEvent*> uiSpikeEvents;
    for (const ProfileEvent& event : session.events) {
        if (event.eventName == QLatin1String("spike_ui_flush") && event.durationUs >= 0) {
            uiSpikeEvents.push_back(&event);
        }
    }
    std::sort(uiSpikeEvents.begin(),
              uiSpikeEvents.end(),
              [](const ProfileEvent* a, const ProfileEvent* b) {
                  return a->durationUs > b->durationUs;
              });
    const int uiLimit = std::min<int>(10, static_cast<int>(uiSpikeEvents.size()));
    if (uiLimit == 0) {
        lines << QStringLiteral("| — | — | — |");
    } else {
        for (int i = 0; i < uiLimit; ++i) {
            const ProfileEvent& event = *uiSpikeEvents[static_cast<size_t>(i)];
            lines << QStringLiteral("| %1 | %2 | %3 |")
                         .arg(formatMs(event.relUs))
                         .arg(formatMs(event.durationUs))
                         .arg(escapeMd(event.detail));
        }
    }
    appendEventSeriesTable(lines, session);
    lines << QString();
    lines << QStringLiteral("## Analysis hints");
    lines << QString();
    lines << QStringLiteral(
        "- **Feature settings** / **Workflow blocks**: frozen at `session_begin` — do not ask the user to describe the workflow.");
    lines << QStringLiteral(
        "- **Auto diagnosis**: Korean summary from snapshot + measured `block_end` / events.");
    lines << QStringLiteral(
        "- **app_trace_begin** / **app_trace_end**: full app lifetime while profiling is enabled.");
    lines << QStringLiteral(
        "- **session_begin** / **session_end**: feature run window inside the app trace.");
    lines << QStringLiteral(
        "- **loop_gap**: `loop_end` → next `loop_begin` idle time (scheduling / UI-thread stalls).");
    lines << QStringLiteral(
        "- **profile_switch** / **load_profile** / **auto_save** / **hotkey_***: app-wide UI and I/O paths.");
    lines << QStringLiteral(
        "- **capture_imagefind** / **imagefind_poll**: screen capture and template matching on the worker thread.");
    lines << QStringLiteral(
        "- **mouse_click**: `InputSimulator` click injection on the worker thread.");
    lines << QStringLiteral(
        "- Attach this `.md` path in Cursor chat and ask for stutter root-cause analysis.");
    lines << QString();
    lines << QStringLiteral("## Event log");
    lines << QString();
    lines << QStringLiteral(
        "Microsecond TSV columns: `rel_us`, `thread`, `event`, `detail`, `dur_us` (blank when N/A).");
    lines << QString();
    lines << QStringLiteral("```tsv");
    lines << QStringLiteral("rel_us\tthread\tevent\tdetail\tdur_us");
    return lines.join(QLatin1Char('\n'));
}

bool writeSessionFile(const SessionState& session, const QString& path, const QString& endReason) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning().noquote() << "WorkflowRunProfiler: failed to write" << path << file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << buildMarkdownReport(session, endReason) << '\n';

    for (const ProfileEvent& event : session.events) {
        stream << event.relUs << '\t' << event.threadTag << '\t' << event.eventName << '\t'
               << event.detail;
        if (event.durationUs >= 0) {
            stream << '\t' << event.durationUs;
        }
        stream << '\n';
    }

    stream << "```\n";
    return true;
}

void writeSessionOutputs(SessionState session, const QString& endReason) {
    const QString directory = WorkflowRunProfiler::outputDirectory();
    const QString sessionsDir = QDir(directory).filePath(QStringLiteral("sessions"));
    QDir().mkpath(sessionsDir);

    const QString timestamp =
        QDateTime::fromMSecsSinceEpoch(session.sessionStartEpochMs).toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString token =
        sanitizeFileToken(session.featureName.isEmpty() ? QStringLiteral("app") : session.featureName);
    const QString stampedPath =
        QDir(sessionsDir).filePath(QStringLiteral("session_%1_%2.md").arg(timestamp, token));
    const QString latestPath = WorkflowRunProfiler::latestLogPath();

    writeSessionFile(session, stampedPath, endReason);
    writeSessionFile(session, latestPath, endReason);
}

void pushEventLocked(SessionState& session,
                     const char* eventName,
                     const QString& detail,
                     qint64 durationUs = -1) {
    if (!session.appActive) {
        return;
    }
    if (static_cast<int>(session.events.size()) >= kMaxEventLines) {
        ++session.droppedEvents;
        return;
    }

    ProfileEvent event;
    event.relUs = queryPerformanceCounterUs() - session.originUs;
    event.threadTag = QString::fromUtf8(threadTagForCurrentThread());
    event.eventName = QString::fromUtf8(eventName);
    event.detail = detail;
    event.durationUs = durationUs;
    session.events.push_back(std::move(event));
}

void resetFeatureSessionMetrics(SessionState& session) {
    session.loops.clear();
    session.loopGapSamples.clear();
    session.uiFlushDurationsUs.clear();
    session.clickDurationsUs.clear();
    session.lastLoopEndRelUs = -1;
    session.lastLoopIteration = 0;
    session.loopBeginCount = 0;
    session.loopEndCount = 0;
    session.blockEndCount = 0;
    session.clickCount = 0;
    session.uiFlushCount = 0;
    session.startSource.clear();
    session.workflowBlockCount = 0;
    session.featureSettingsLines.clear();
    session.blockSnapshots.clear();
    session.blockAggregates.clear();
    session.featureSessionBeginRelUs = 0;
}

std::vector<ProfileEventLite> featureSessionEvents(const SessionState& session) {
    std::vector<ProfileEventLite> window;
    qint64 windowBeginUs = session.featureSessionBeginRelUs;
    if (windowBeginUs < 0) {
        for (const ProfileEvent& event : session.events) {
            if (event.eventName == QLatin1String("session_begin")) {
                windowBeginUs = event.relUs;
                break;
            }
        }
    }
    for (const ProfileEvent& event : session.events) {
        if (windowBeginUs >= 0 && event.relUs < windowBeginUs) {
            continue;
        }
        ProfileEventLite lite;
        lite.relUs = event.relUs;
        lite.eventName = event.eventName;
        lite.detail = event.detail;
        lite.durationUs = event.durationUs;
        window.push_back(std::move(lite));
    }
    return window;
}

std::vector<BlockRunMeasuredRow> measuredRowsFromAggregates(const SessionState& session) {
    std::vector<BlockRunMeasuredRow> rows;
    rows.reserve(session.blockAggregates.size());
    for (const BlockRunAggregate& aggregate : session.blockAggregates) {
        BlockRunMeasuredRow row;
        row.oneBasedIndex = aggregate.oneBasedIndex;
        row.type = aggregate.type;
        row.config = aggregate.config;
        row.execCount = aggregate.execCount;
        row.successCount = aggregate.successCount;
        row.failCount = aggregate.failCount;
        row.totalDurationUs = aggregate.totalDurationUs;
        row.maxDurationUs = aggregate.maxDurationUs;
        rows.push_back(std::move(row));
    }
    return rows;
}

} // namespace

bool WorkflowRunProfiler::isEnabled() {
    return g_enabled;
}

bool WorkflowRunProfiler::isAppTraceActive() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_trace.appActive;
}

void WorkflowRunProfiler::reloadEnabledFromSettings() {
    const bool fromEnv = qEnvironmentVariableIntValue("PIPBONG_WORKFLOW_PROFILE") != 0
                         || qEnvironmentVariableIntValue("PIPBONG_APP_PROFILE") != 0;
    const bool wantEnabled = fromEnv || ProgramSettings::workflowRunProfiling();
#ifdef _WIN32
    g_mainThreadId = GetCurrentThreadId();
#endif
    if (wantEnabled && !g_enabled) {
        g_enabled = true;
        beginAppTrace();
        return;
    }
    if (!wantEnabled && g_enabled) {
        g_enabled = false;
        endAppTrace(QStringLiteral("settings_disabled"));
        return;
    }
    g_enabled = wantEnabled;
}

void WorkflowRunProfiler::beginAppTrace() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_trace.appActive) {
        return;
    }
    g_trace = SessionState{};
    g_trace.appActive = true;
    g_trace.originUs = monotonicUs();
    g_trace.sessionStartEpochMs = QDateTime::currentMSecsSinceEpoch();
    pushEventLocked(g_trace, "app_trace_begin", QStringLiteral("version=%1").arg(QStringLiteral(PIPBONG_VERSION)));
}

void WorkflowRunProfiler::endAppTrace(const QString& reason) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive) {
        return;
    }
    if (g_trace.featureSessionActive) {
        pushEventLocked(g_trace, "session_end", QStringLiteral("reason=%1").arg(reason));
        g_trace.featureSessionActive = false;
    }
    pushEventLocked(g_trace, "app_trace_end", reason);
    const SessionState snapshot = std::move(g_trace);
    g_trace = SessionState{};
    writeSessionOutputs(snapshot, reason);
}

QString WorkflowRunProfiler::outputDirectory() {
    return QDir(findRepositoryRoot()).filePath(QStringLiteral("workflow-profile"));
}

QString WorkflowRunProfiler::latestLogPath() {
    return QDir(outputDirectory()).filePath(QStringLiteral("latest.md"));
}

void WorkflowRunProfiler::beginSession(const QString& featureId,
                                       const QString& featureName,
                                       const QString& runMode,
                                       const QString& profileName,
                                       const Feature* feature,
                                       const QString& startSource) {
    reloadEnabledFromSettings();
    if (!g_enabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive) {
        g_trace.appActive = true;
        g_trace.originUs = monotonicUs();
        g_trace.sessionStartEpochMs = QDateTime::currentMSecsSinceEpoch();
        pushEventLocked(g_trace, "app_trace_begin", QStringLiteral("version=%1").arg(QStringLiteral(PIPBONG_VERSION)));
    }

    if (g_trace.featureSessionActive) {
        pushEventLocked(g_trace,
                        "session_end",
                        QStringLiteral("reason=preempted_by_new_session"));
        writeSessionOutputs(g_trace, QStringLiteral("preempted_by_new_session"));
        resetFeatureSessionMetrics(g_trace);
    }

    g_trace.featureSessionActive = true;
    g_trace.featureId = featureId;
    g_trace.featureName = featureName;
    g_trace.runMode = runMode;
    g_trace.profileName = profileName;
    g_trace.startSource = startSource;
    g_trace.featureSessionBeginRelUs = monotonicUs() - g_trace.originUs;

    if (feature != nullptr) {
        const WorkflowProfileSnapshotData snapshot = captureWorkflowProfileSnapshot(*feature);
        g_trace.featureSettingsLines = snapshot.featureSettings;
        g_trace.blockSnapshots = snapshot.blocks;
        g_trace.workflowBlockCount = snapshot.blockCount;
        g_trace.blockAggregates.clear();
        g_trace.blockAggregates.reserve(snapshot.blocks.size());
        for (const WorkflowBlockSnapshotRow& row : snapshot.blocks) {
            BlockRunAggregate aggregate;
            aggregate.oneBasedIndex = row.oneBasedIndex;
            aggregate.type = row.type;
            aggregate.config = row.config;
            g_trace.blockAggregates.push_back(std::move(aggregate));
        }
    }

    QString beginDetail = QStringLiteral("feature=%1 mode=%2 profile=%3").arg(featureName, runMode, profileName);
    if (!startSource.isEmpty()) {
        beginDetail += QStringLiteral(" source=%1").arg(startSource);
    }
    if (g_trace.workflowBlockCount > 0) {
        beginDetail += QStringLiteral(" blocks=%1").arg(g_trace.workflowBlockCount);
    }
    pushEventLocked(g_trace, "session_begin", beginDetail, -1);
}

void WorkflowRunProfiler::endSession(const QString& reason) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive || !g_trace.featureSessionActive) {
        return;
    }

    pushEventLocked(g_trace, "session_end", reason);
    g_trace.featureSessionActive = false;
    writeSessionOutputs(g_trace, reason);
}

void WorkflowRunProfiler::event(const char* eventName, const QString& detail, qint64 durationUs) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive) {
        return;
    }
    pushEventLocked(g_trace, eventName, detail, durationUs);
    if (durationUs >= 0 && std::strcmp(eventName, "mouse_click") == 0) {
        g_trace.clickDurationsUs.push_back(durationUs);
        ++g_trace.clickCount;
    }
}

void WorkflowRunProfiler::recordLoopBegin(int iteration) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive || !g_trace.featureSessionActive) {
        return;
    }
    const qint64 relUs = monotonicUs() - g_trace.originUs;
    if (g_trace.lastLoopEndRelUs >= 0) {
        const qint64 gapUs = relUs - g_trace.lastLoopEndRelUs;
        LoopGapSample gapSample;
        gapSample.gapUs = gapUs;
        gapSample.relUs = relUs;
        gapSample.afterLoopIteration = g_trace.lastLoopIteration;
        g_trace.loopGapSamples.push_back(gapSample);
        if (gapUs >= kSpikeLoopGapUs) {
            pushEventLocked(g_trace,
                            "spike_loop_gap",
                            QStringLiteral("after_loop=%1 gap_us=%2")
                                .arg(g_trace.lastLoopIteration)
                                .arg(gapUs),
                            gapUs);
        }
    }
    ++g_trace.loopBeginCount;
    pushEventLocked(g_trace,
                    "loop_begin",
                    QStringLiteral("iter=%1").arg(iteration));
}

void WorkflowRunProfiler::recordLoopEnd(int iteration, qint64 durationUs) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive || !g_trace.featureSessionActive) {
        return;
    }
    const qint64 relUs = monotonicUs() - g_trace.originUs;
    g_trace.lastLoopEndRelUs = relUs;
    g_trace.lastLoopIteration = iteration;
    ++g_trace.loopEndCount;

    LoopStats loop;
    loop.iteration = iteration;
    loop.durationUs = durationUs;
    loop.relEndUs = relUs;
    g_trace.loops.push_back(loop);

    pushEventLocked(g_trace,
                    "loop_end",
                    QStringLiteral("iter=%1").arg(iteration),
                    durationUs);
    if (durationUs >= kSpikeLoopDurationUs) {
        pushEventLocked(g_trace,
                        "spike_loop_duration",
                        QStringLiteral("iter=%1").arg(iteration),
                        durationUs);
    }
}

void WorkflowRunProfiler::recordBlockEnd(int blockIndex,
                                         const char* blockType,
                                         bool success,
                                         qint64 durationUs) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive || !g_trace.featureSessionActive) {
        return;
    }
    ++g_trace.blockEndCount;
    if (blockIndex >= 0 && blockIndex < static_cast<int>(g_trace.blockAggregates.size())) {
        BlockRunAggregate& aggregate = g_trace.blockAggregates[static_cast<size_t>(blockIndex)];
        ++aggregate.execCount;
        if (success) {
            ++aggregate.successCount;
        } else {
            ++aggregate.failCount;
        }
        aggregate.totalDurationUs += durationUs;
        aggregate.maxDurationUs = std::max(aggregate.maxDurationUs, durationUs);
    }
    pushEventLocked(g_trace,
                    "block_end",
                    QStringLiteral("#%1 type=%2 success=%3")
                        .arg(blockIndex + 1)
                        .arg(QString::fromUtf8(blockType))
                        .arg(success ? QStringLiteral("yes") : QStringLiteral("no")),
                    durationUs);
}

void WorkflowRunProfiler::recordLoopIntervalSleep(int delayMs, qint64 actualSleepUs) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive || !g_trace.featureSessionActive) {
        return;
    }
    pushEventLocked(g_trace,
                    "loop_interval_sleep",
                    QStringLiteral("requested_ms=%1").arg(delayMs),
                    actualSleepUs);
}

void WorkflowRunProfiler::recordUiFlush(int pendingIterations, qint64 durationUs) {
    if (!g_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_trace.appActive || !g_trace.featureSessionActive) {
        return;
    }
    ++g_trace.uiFlushCount;
    g_trace.uiFlushDurationsUs.push_back(durationUs);
    pushEventLocked(g_trace,
                    "ui_fast_repeat_flush",
                    QStringLiteral("pending_iters=%1").arg(pendingIterations),
                    durationUs);
    if (durationUs >= kSpikeUiFlushUs) {
        pushEventLocked(g_trace,
                        "spike_ui_flush",
                        QStringLiteral("pending_iters=%1").arg(pendingIterations),
                        durationUs);
    }
}

void WorkflowRunProfiler::flushPendingSessions(const QString& reason) {
    endAppTrace(reason);
}

qint64 WorkflowRunProfiler::monotonicUs() {
    return queryPerformanceCounterUs();
}

WfProfileScope::WfProfileScope(const char* eventName, QString detail)
    : m_eventName(eventName)
    , m_detail(std::move(detail)) {
    if (!WorkflowRunProfiler::isEnabled()) {
        return;
    }
    m_active = true;
    m_startUs = WorkflowRunProfiler::monotonicUs();
}

WfProfileScope::~WfProfileScope() {
    if (!m_active || m_eventName == nullptr) {
        return;
    }
    const qint64 durationUs = WorkflowRunProfiler::monotonicUs() - m_startUs;
    WorkflowRunProfiler::event(m_eventName, m_detail, durationUs);
}