#pragma once

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
        const bool enabled = true;
#else
        const bool enabled = QLoggingCategory("pipbong.perf.trace").isDebugEnabled();
#endif
        if (!enabled) {
            return;
        }
        qCDebug(perfTrace).noquote() << m_label << m_timer.elapsed() << "ms";
    }

    qint64 elapsedMs() const { return m_timer.elapsed(); }

private:
    const char* m_label = nullptr;
    QElapsedTimer m_timer;
};

#define PIPBONG_PERF_SCOPE(label) PerfTrace perfTraceScope_##__LINE__(label)
