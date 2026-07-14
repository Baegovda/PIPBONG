#pragma once

#include "core/diagnostics/CpuMonitorTypes.h"

#include <QDateTime>
#include <QHash>
#include <QVector>
#include <deque>
#include <optional>

class CpuSpikeDetector {
public:
    void setConfig(const CpuSpikeDetectorConfig& config);
    const CpuSpikeDetectorConfig& config() const { return m_config; }

    std::optional<CpuSpikeEvent> evaluate(const CpuSampleSnapshot& sample, bool pipbongFeatureRunning);

private:
    static double medianOf(const std::deque<double>& values);
    static QVector<ProcessCpuEntry> topSnapshot(const QVector<ProcessCpuEntry>& processes, int count);
    bool inCooldown(const QString& key, const QDateTime& now) const;
    void markCooldown(const QString& key, const QDateTime& now);

    CpuSpikeDetectorConfig m_config;
    std::deque<double> m_systemHistory;
    QHash<quint32, std::deque<double>> m_processHistory;
    QHash<QString, QDateTime> m_lastEventAt;
};
