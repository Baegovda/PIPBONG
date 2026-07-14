#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

struct ProcessCpuEntry {
    quint32 pid = 0;
    QString name;
    QString executablePath;
    double cpuPercent = 0.0;
    bool accessible = true;
};

struct CpuSampleSnapshot {
    QDateTime timestamp;
    double systemCpuPercent = 0.0;
    bool systemReady = false;
    QVector<ProcessCpuEntry> topProcesses;
};

enum class SpikeTriggerKind {
    SystemAbsolute,
    ProcessAbsolute,
    SystemRelative,
    ProcessRelative,
};

struct CpuSpikeEvent {
    QDateTime timestamp;
    SpikeTriggerKind triggerKind = SpikeTriggerKind::SystemAbsolute;
    QString triggerDetail;
    double systemCpuPercent = 0.0;
    QVector<ProcessCpuEntry> topProcesses;
    bool pipbongFeatureRunning = false;
};

struct CpuSpikeDetectorConfig {
    double systemThresholdPercent = 70.0;
    double processThresholdPercent = 40.0;
    double deltaMarginPercent = 15.0;
    int cooldownMs = 2000;
    int baselineWindowSamples = 30;
};

Q_DECLARE_METATYPE(CpuSampleSnapshot)
Q_DECLARE_METATYPE(CpuSpikeEvent)
Q_DECLARE_METATYPE(CpuSpikeDetectorConfig)
