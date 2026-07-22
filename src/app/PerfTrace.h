#pragma once

#include "core/diagnostics/WorkflowRunProfiler.h"

#include <QElapsedTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(perfTrace)

/// Lightweight scoped timers for profile-switch and UI hot paths.
/// Enable with QT_LOGGING_RULES="pipbong.perf.trace=true" or in Debug builds.
class PerfTrace {
public:
    explicit PerfTrace(const char* label)
        : m_label(label) {
        m_timer.start();
    }

    ~PerfTrace() {
#if defined(QT_DEBUG)
        const bool logEnabled = true;
#else
        const bool logEnabled = QLoggingCategory("pipbong.perf.trace").isDebugEnabled();
#endif
        const qint64 elapsedMs = m_timer.elapsed();
        if (WorkflowRunProfiler::isEnabled()) {
            WorkflowRunProfiler::event(
                m_label,
                QStringLiteral("elapsed_ms=%1").arg(elapsedMs),
                elapsedMs * 1000);
        }
        if (logEnabled) {
            qCDebug(perfTrace).noquote() << m_label << elapsedMs << "ms";
        }
    }

    qint64 elapsedMs() const { return m_timer.elapsed(); }

private:
    const char* m_label = nullptr;
    QElapsedTimer m_timer;
};

#define PIPBONG_PERF_SCOPE(label) PerfTrace perfTraceScope_##__LINE__(label)
