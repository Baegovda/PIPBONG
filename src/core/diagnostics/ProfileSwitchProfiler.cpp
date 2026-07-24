#include "core/diagnostics/ProfileSwitchProfiler.h"

#include "app/ProgramSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

namespace {

constexpr int kMaxPhaseRows = 256;

struct PhaseRow {
    qint64 relMs = 0;
    QString phase;
    qint64 durationMs = -1;
    QString detail;
};

std::mutex g_mutex;
bool g_switchActive = false;
QString g_targetProfileId;
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

QString outputDirectory() {
    const QString local = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(local).filePath(QStringLiteral("profile-switch"));
}

QString latestReportPath() {
    return QDir(outputDirectory()).filePath(QStringLiteral("latest.md"));
}

void trimPhasesLocked() {
    if (static_cast<int>(g_phases.size()) <= kMaxPhaseRows) {
        return;
    }
    const int drop = static_cast<int>(g_phases.size()) - kMaxPhaseRows;
    g_phases.erase(g_phases.begin(), g_phases.begin() + drop);
}

} // namespace

bool ProfileSwitchProfiler::isEnabled() {
    return ProgramSettings::profileSwitchProfiling() || envEnabled();
}

void ProfileSwitchProfiler::beginSwitch(const QString& targetProfileId) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_switchActive = true;
    g_targetProfileId = targetProfileId;
    g_switchStartMs = steadyMs();
    g_pingpongCount = 0;
    g_startTimerWarnings = 0;
    g_phases.clear();
    g_phases.push_back({0, QStringLiteral("begin"), -1, targetProfileId});
}

void ProfileSwitchProfiler::notePhase(const QString& phase,
                                      qint64 durationMs,
                                      const QString& detail) {
    if (!isEnabled()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_switchActive && phase != QStringLiteral("requested")) {
        g_switchActive = true;
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

void ProfileSwitchProfiler::flushReport(const QString& reason) {
    if (!isEnabled()) {
        return;
    }

    QString reportBody;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_switchActive && g_phases.empty()) {
            return;
        }

        const QString version = QCoreApplication::applicationVersion();
        reportBody += QStringLiteral("# Profile switch report\n\n");
        reportBody += QStringLiteral("- Generated: %1\n")
                          .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        reportBody += QStringLiteral("- Reason: %1\n").arg(reason);
        reportBody += QStringLiteral("- App version: %1\n").arg(version);
        if (!g_targetProfileId.isEmpty()) {
            reportBody += QStringLiteral("- Target profile: `%1`\n").arg(g_targetProfileId);
        }
        reportBody += QStringLiteral("- Ping-pong requests: %1\n").arg(g_pingpongCount);
        reportBody += QStringLiteral("- startTimer cross-thread warnings: %1\n\n")
                          .arg(g_startTimerWarnings);
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

        g_switchActive = false;
        g_phases.clear();
        g_targetProfileId.clear();
        g_switchStartMs = 0;
        g_pingpongCount = 0;
        g_startTimerWarnings = 0;
    }

    const QString dirPath = outputDirectory();
    QDir().mkpath(dirPath);
    QFile file(latestReportPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        file.write(reportBody.toUtf8());
    }
}
