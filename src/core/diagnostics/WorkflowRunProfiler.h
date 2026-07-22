#pragma once

#include "app/ProgramSettings.h"

#include <QString>
#include <QStringList>

#include <cstdint>

class Feature;

/// High-resolution app + workflow profiler for stutter / latency diagnosis.
/// Enable via program setting or PIPBONG_APP_PROFILE=1 / PIPBONG_WORKFLOW_PROFILE=1.
/// Writes `<repo-root>/workflow-profile/latest.md` on feature session end and app shutdown.
class WorkflowRunProfiler {
public:
    using ProfilingDepth = ProgramSettings::WorkflowRunProfilingDepth;

    static bool isEnabled();
    static bool isAppTraceActive();
    static ProfilingDepth depth();
    static int currentLoopIteration();
    static void reloadEnabledFromSettings();

    static QString outputDirectory();
    static QString latestLogPath();
    static QString latestChromeTracePath();

    /// Starts recording from app launch (or when profiling is toggled on).
    static void beginAppTrace();
    static void endAppTrace(const QString& reason);

    static void beginSession(const QString& featureId,
                             const QString& featureName,
                             const QString& runMode,
                             const QString& profileName,
                             const Feature* feature = nullptr,
                             const QString& startSource = QString(),
                             const QStringList& profileContextLines = QStringList());
    static void endSession(const QString& reason);

    static void event(const char* eventName,
                      const QString& detail = QString(),
                      qint64 durationUs = -1);

    static void recordLoopBegin(int iteration);
    static void recordLoopEnd(int iteration, qint64 durationUs);
    static void recordBlockEnd(int blockIndex,
                               const char* blockType,
                               bool success,
                               qint64 durationUs);
    static void recordLoopIntervalSleep(int delayMs, qint64 actualSleepUs);
    static void recordUiFlush(int pendingIterations, qint64 durationUs);

    /// Physical keyboard/mouse detected by UserInputInterruptMonitor (not PIPBONG-injected).
    static void recordPhysicalInput(const char* channel, int virtualKey);

    /// Foreground top-level window title change (Detailed+).
    static void recordForegroundChange(const QString& windowTitle, const QString& reason = QString());

    /// Per ImageFind poll attempt (depth-filtered).
    static void recordImageFindPoll(int blockIndex,
                                    int pollNum,
                                    bool matched,
                                    double confidence,
                                    qint64 captureUs,
                                    qint64 matchUs,
                                    const QString& extra = QString());

    static bool shouldRecordImageFindPoll(bool matched, qint64 totalPollUs, int pollNum);

    static void flushPendingSessions(const QString& reason);

    /// High-resolution monotonic clock for profiling scopes (microseconds).
    static qint64 monotonicUs();
};

/// RAII scope — no-op when profiling is disabled.
class WfProfileScope {
public:
    WfProfileScope(const char* eventName, QString detail = QString());
    ~WfProfileScope();

private:
    const char* m_eventName = nullptr;
    QString m_detail;
    qint64 m_startUs = 0;
    bool m_active = false;
};

#define PIPBONG_PROFILE(event, detail) WfProfileScope wfProfileScope_##__LINE__(event, detail)
#define PIPBONG_PROFILE_CAT(event, detailExpr) \
    WfProfileScope wfProfileScope_##__LINE__(event, detailExpr)
#define PIPBONG_WF_PROFILE(event, detail) PIPBONG_PROFILE(event, detail)
#define PIPBONG_WF_PROFILE_CAT(event, detailExpr) PIPBONG_PROFILE_CAT(event, detailExpr)
