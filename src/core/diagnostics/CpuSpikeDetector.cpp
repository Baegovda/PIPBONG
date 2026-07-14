#include "core/diagnostics/CpuSpikeDetector.h"

#include <algorithm>
#include <cmath>

namespace {

QString triggerKindLabel(SpikeTriggerKind kind) {
    switch (kind) {
    case SpikeTriggerKind::SystemAbsolute:
        return QStringLiteral("system_absolute");
    case SpikeTriggerKind::ProcessAbsolute:
        return QStringLiteral("process_absolute");
    case SpikeTriggerKind::SystemRelative:
        return QStringLiteral("system_relative");
    case SpikeTriggerKind::ProcessRelative:
        return QStringLiteral("process_relative");
    }
    return QStringLiteral("unknown");
}

} // namespace

void CpuSpikeDetector::setConfig(const CpuSpikeDetectorConfig& config) {
    m_config = config;
}

double CpuSpikeDetector::medianOf(const std::deque<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    std::vector<double> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());
    const size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0) {
        return (sorted[mid - 1] + sorted[mid]) / 2.0;
    }
    return sorted[mid];
}

QVector<ProcessCpuEntry> CpuSpikeDetector::topSnapshot(const QVector<ProcessCpuEntry>& processes,
                                                         int count) {
    QVector<ProcessCpuEntry> copy = processes;
    std::sort(copy.begin(), copy.end(), [](const ProcessCpuEntry& a, const ProcessCpuEntry& b) {
        return a.cpuPercent > b.cpuPercent;
    });
    if (copy.size() > count) {
        copy.resize(count);
    }
    return copy;
}

bool CpuSpikeDetector::inCooldown(const QString& key, const QDateTime& now) const {
    const auto it = m_lastEventAt.constFind(key);
    if (it == m_lastEventAt.constEnd()) {
        return false;
    }
    return it.value().msecsTo(now) < m_config.cooldownMs;
}

void CpuSpikeDetector::markCooldown(const QString& key, const QDateTime& now) {
    m_lastEventAt.insert(key, now);
}

std::optional<CpuSpikeEvent> CpuSpikeDetector::evaluate(const CpuSampleSnapshot& sample,
                                                        bool pipbongFeatureRunning) {
    if (!sample.systemReady) {
        return std::nullopt;
    }

    const QDateTime now = sample.timestamp.isValid() ? sample.timestamp : QDateTime::currentDateTime();
    const double systemCpu = sample.systemCpuPercent;

    m_systemHistory.push_back(systemCpu);
    while (static_cast<int>(m_systemHistory.size()) > m_config.baselineWindowSamples) {
        m_systemHistory.pop_front();
    }

    const double systemBaseline = m_systemHistory.size() >= 5 ? medianOf(m_systemHistory) : 0.0;

    for (const ProcessCpuEntry& entry : sample.topProcesses) {
        auto& history = m_processHistory[entry.pid];
        history.push_back(entry.cpuPercent);
        while (static_cast<int>(history.size()) > m_config.baselineWindowSamples) {
            history.pop_front();
        }
    }

    struct Candidate {
        SpikeTriggerKind kind = SpikeTriggerKind::SystemAbsolute;
        QString detail;
        QString cooldownKey;
        double score = 0.0;
    };

    std::optional<Candidate> best;

    auto consider = [&](const Candidate& candidate) {
        if (inCooldown(candidate.cooldownKey, now)) {
            return;
        }
        if (!best.has_value() || candidate.score > best->score) {
            best = candidate;
        }
    };

    if (systemCpu >= m_config.systemThresholdPercent) {
        Candidate candidate;
        candidate.kind = SpikeTriggerKind::SystemAbsolute;
        candidate.detail = QStringLiteral("system CPU %1% >= %2%")
                               .arg(QString::number(systemCpu, 'f', 1))
                               .arg(QString::number(m_config.systemThresholdPercent, 'f', 0));
        candidate.cooldownKey =
            triggerKindLabel(SpikeTriggerKind::SystemAbsolute) + QStringLiteral("|system");
        candidate.score = systemCpu;
        consider(candidate);
    }

    for (const ProcessCpuEntry& entry : sample.topProcesses) {
        if (entry.cpuPercent >= m_config.processThresholdPercent) {
            Candidate candidate;
            candidate.kind = SpikeTriggerKind::ProcessAbsolute;
            candidate.detail = QStringLiteral("%1 (PID %2) CPU %3% >= %4%")
                                   .arg(entry.name)
                                   .arg(entry.pid)
                                   .arg(QString::number(entry.cpuPercent, 'f', 1))
                                   .arg(QString::number(m_config.processThresholdPercent, 'f', 0));
            candidate.cooldownKey = triggerKindLabel(SpikeTriggerKind::ProcessAbsolute)
                                    + QStringLiteral("|") + QString::number(entry.pid);
            candidate.score = entry.cpuPercent;
            consider(candidate);
        }
    }

    if (m_config.deltaMarginPercent > 0.0 && m_systemHistory.size() >= 5) {
        if (systemCpu > systemBaseline + m_config.deltaMarginPercent) {
            Candidate candidate;
            candidate.kind = SpikeTriggerKind::SystemRelative;
            candidate.detail = QStringLiteral("system CPU %1% > baseline %2% + %3%")
                                   .arg(QString::number(systemCpu, 'f', 1))
                                   .arg(QString::number(systemBaseline, 'f', 1))
                                   .arg(QString::number(m_config.deltaMarginPercent, 'f', 0));
            candidate.cooldownKey =
                triggerKindLabel(SpikeTriggerKind::SystemRelative) + QStringLiteral("|system");
            candidate.score = systemCpu - systemBaseline;
            consider(candidate);
        }
    }

    if (m_config.deltaMarginPercent > 0.0) {
        for (const ProcessCpuEntry& entry : sample.topProcesses) {
            const auto it = m_processHistory.constFind(entry.pid);
            if (it == m_processHistory.constEnd() || it.value().size() < 5) {
                continue;
            }
            const double baseline = medianOf(it.value());
            if (entry.cpuPercent > baseline + m_config.deltaMarginPercent) {
                Candidate candidate;
                candidate.kind = SpikeTriggerKind::ProcessRelative;
                candidate.detail = QStringLiteral("%1 (PID %2) CPU %3% > baseline %4% + %5%")
                                       .arg(entry.name)
                                       .arg(entry.pid)
                                       .arg(QString::number(entry.cpuPercent, 'f', 1))
                                       .arg(QString::number(baseline, 'f', 1))
                                       .arg(QString::number(m_config.deltaMarginPercent, 'f', 0));
                candidate.cooldownKey = triggerKindLabel(SpikeTriggerKind::ProcessRelative)
                                        + QStringLiteral("|") + QString::number(entry.pid);
                candidate.score = entry.cpuPercent - baseline;
                consider(candidate);
            }
        }
    }

    if (!best.has_value()) {
        return std::nullopt;
    }

    markCooldown(best->cooldownKey, now);

    CpuSpikeEvent event;
    event.timestamp = now;
    event.triggerKind = best->kind;
    event.triggerDetail = best->detail;
    event.systemCpuPercent = systemCpu;
    event.topProcesses = topSnapshot(sample.topProcesses, 5);
    event.pipbongFeatureRunning = pipbongFeatureRunning;
    return event;
}
