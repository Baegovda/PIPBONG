#include "core/diagnostics/CpuCulpritAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kBaselineSampleTarget = 20;
constexpr double kSystemElevationMarginPercent = 12.0;

} // namespace

void CpuCulpritAnalyzer::reset() {
    m_stats.clear();
    m_systemBaselineSamples.clear();
    m_sessionSystemBaseline = 0.0;
    m_hasSystemBaseline = false;
    m_cachedRanking.clear();
    m_rankingDirty = true;
}

CpuCulpritAnalyzer::ProcessStats& CpuCulpritAnalyzer::statsFor(const ProcessCpuEntry& entry) {
    ProcessStats& stats = m_stats[entry.pid];
    if (stats.pid == 0) {
        stats.pid = entry.pid;
    }
    if (!entry.name.isEmpty()) {
        stats.name = entry.name;
    }
    if (!entry.executablePath.isEmpty()) {
        stats.executablePath = entry.executablePath;
    }
    return stats;
}

double CpuCulpritAnalyzer::spikeRankWeight(int rankIndex) {
    switch (rankIndex) {
    case 0:
        return 5.0;
    case 1:
        return 3.0;
    case 2:
        return 2.0;
    case 3:
        return 1.5;
    default:
        return 1.0;
    }
}

void CpuCulpritAnalyzer::recordSample(const CpuSampleSnapshot& sample,
                                      double processThresholdPercent) {
    if (!sample.systemReady) {
        return;
    }

    if (static_cast<int>(m_systemBaselineSamples.size()) < kBaselineSampleTarget) {
        m_systemBaselineSamples.push_back(sample.systemCpuPercent);
        if (static_cast<int>(m_systemBaselineSamples.size()) == kBaselineSampleTarget) {
            QVector<double> sorted = m_systemBaselineSamples;
            std::sort(sorted.begin(), sorted.end());
            m_sessionSystemBaseline = sorted[sorted.size() / 2];
            m_hasSystemBaseline = true;
        }
    }

    const bool systemElevated =
        m_hasSystemBaseline
        && sample.systemCpuPercent > m_sessionSystemBaseline + kSystemElevationMarginPercent;

    for (int rank = 0; rank < sample.topProcesses.size(); ++rank) {
        const ProcessCpuEntry& entry = sample.topProcesses.at(rank);
        ProcessStats& stats = statsFor(entry);

        ++stats.sampleCount;
        stats.sumCpuPercent += entry.cpuPercent;
        stats.maxCpuPercent = std::max(stats.maxCpuPercent, entry.cpuPercent);

        if (rank == 0) {
            ++stats.top1SampleCount;
        }

        if (systemElevated || entry.cpuPercent >= processThresholdPercent) {
            const double loadFactor = sample.systemCpuPercent / 100.0;
            stats.elevationScore += entry.cpuPercent * std::max(0.15, loadFactor);
        }
    }

    m_rankingDirty = true;
}

void CpuCulpritAnalyzer::recordSpike(const CpuSpikeEvent& event) {
    const bool processTriggered =
        event.triggerKind == SpikeTriggerKind::ProcessAbsolute
        || event.triggerKind == SpikeTriggerKind::ProcessRelative;

    for (int rank = 0; rank < event.topProcesses.size(); ++rank) {
        const ProcessCpuEntry& entry = event.topProcesses.at(rank);
        ProcessStats& stats = statsFor(entry);
        const double weight = spikeRankWeight(rank);

        ++stats.spikeInvolvementCount;
        stats.weightedSpikeCpuSum += entry.cpuPercent * weight;

        if (rank == 0) {
            ++stats.spikeTop1Count;
            if (processTriggered) {
                ++stats.spikeTriggerCount;
            }
        }
    }

    m_rankingDirty = true;
}

QString CpuCulpritAnalyzer::buildEvidenceSummary(const ProcessStats& stats) {
    QStringList parts;

    if (stats.spikeTriggerCount > 0) {
        parts << QStringLiteral("직접 트리거 %1회").arg(stats.spikeTriggerCount);
    }
    if (stats.spikeTop1Count > 0) {
        parts << QStringLiteral("스파이크 1위 %1회").arg(stats.spikeTop1Count);
    }
    if (stats.spikeInvolvementCount > 0) {
        parts << QStringLiteral("스파이크 관여 %1회").arg(stats.spikeInvolvementCount);
    }
    if (stats.top1SampleCount > 0) {
        parts << QStringLiteral("샘플 1위 %1회").arg(stats.top1SampleCount);
    }
    if (stats.maxCpuPercent > 0.0) {
        parts << QStringLiteral("최고 CPU %1%")
                     .arg(QString::number(stats.maxCpuPercent, 'f', 1));
    }
    if (stats.sampleCount > 0) {
        const double average = stats.sumCpuPercent / static_cast<double>(stats.sampleCount);
        parts << QStringLiteral("평균 CPU %1%").arg(QString::number(average, 'f', 1));
    }

    if (parts.isEmpty()) {
        return QStringLiteral("—");
    }
    return parts.join(QStringLiteral(" · "));
}

void CpuCulpritAnalyzer::recomputeScores() {
    double maxRaw = 0.0;
    for (auto it = m_stats.begin(); it != m_stats.end(); ++it) {
        ProcessStats& stats = it.value();
        const double avgSpikeCpu = stats.spikeInvolvementCount > 0
                                       ? stats.weightedSpikeCpuSum
                                             / static_cast<double>(stats.spikeInvolvementCount)
                                       : 0.0;

        stats.rawScore = stats.spikeTop1Count * 35.0 + stats.spikeInvolvementCount * 8.0
                         + stats.spikeTriggerCount * 25.0 + stats.top1SampleCount * 2.0
                         + stats.maxCpuPercent * 1.2 + avgSpikeCpu * 1.5
                         + stats.elevationScore * 0.35;

        maxRaw = std::max(maxRaw, stats.rawScore);
    }

    m_cachedRanking.clear();
    m_cachedRanking.reserve(m_stats.size());

    for (auto it = m_stats.constBegin(); it != m_stats.constEnd(); ++it) {
        const ProcessStats& stats = it.value();
        if (stats.rawScore <= 0.0 && stats.sampleCount <= 0) {
            continue;
        }

        CpuCulpritEntry entry;
        entry.pid = stats.pid;
        entry.name = stats.name;
        entry.executablePath = stats.executablePath;
        entry.evidenceSummary = buildEvidenceSummary(stats);
        if (maxRaw > 0.0) {
            entry.suspicionScore = std::min(100.0, (stats.rawScore / maxRaw) * 100.0);
        } else {
            entry.suspicionScore = 0.0;
        }
        m_cachedRanking.push_back(entry);
    }

    std::sort(m_cachedRanking.begin(), m_cachedRanking.end(),
              [](const CpuCulpritEntry& a, const CpuCulpritEntry& b) {
                  if (!qFuzzyCompare(a.suspicionScore, b.suspicionScore)) {
                      return a.suspicionScore > b.suspicionScore;
                  }
                  return a.pid < b.pid;
              });

    m_rankingDirty = false;
}

QVector<CpuCulpritEntry> CpuCulpritAnalyzer::rankedCulprits(int limit) const {
    if (m_rankingDirty) {
        const_cast<CpuCulpritAnalyzer*>(this)->recomputeScores();
    }

    if (limit <= 0 || m_cachedRanking.size() <= limit) {
        return m_cachedRanking;
    }
    return m_cachedRanking.mid(0, limit);
}
