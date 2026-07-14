#pragma once

#include "core/diagnostics/CpuMonitorTypes.h"

#include <QHash>
#include <QVector>

struct CpuCulpritEntry {
    quint32 pid = 0;
    QString name;
    QString executablePath;
    double suspicionScore = 0.0;
    QString evidenceSummary;
};

class CpuCulpritAnalyzer {
public:
    void reset();

    void recordSample(const CpuSampleSnapshot& sample, double processThresholdPercent);
    void recordSpike(const CpuSpikeEvent& event);

    QVector<CpuCulpritEntry> rankedCulprits(int limit = 8) const;

private:
    struct ProcessStats {
        quint32 pid = 0;
        QString name;
        QString executablePath;
        int sampleCount = 0;
        double sumCpuPercent = 0.0;
        double maxCpuPercent = 0.0;
        int top1SampleCount = 0;
        int spikeInvolvementCount = 0;
        int spikeTop1Count = 0;
        int spikeTriggerCount = 0;
        double weightedSpikeCpuSum = 0.0;
        double elevationScore = 0.0;
        double rawScore = 0.0;
    };

    ProcessStats& statsFor(const ProcessCpuEntry& entry);
    void recomputeScores();
    static QString buildEvidenceSummary(const ProcessStats& stats);
    static double spikeRankWeight(int rankIndex);

    QHash<quint32, ProcessStats> m_stats;
    QVector<double> m_systemBaselineSamples;
    double m_sessionSystemBaseline = 0.0;
    bool m_hasSystemBaseline = false;
    mutable QVector<CpuCulpritEntry> m_cachedRanking;
    mutable bool m_rankingDirty = true;
};
