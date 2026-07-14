#pragma once

#include "core/diagnostics/CpuMonitorTypes.h"
#include "core/diagnostics/CpuSpikeDetector.h"
#include "core/diagnostics/ProcessCpuSampler.h"
#include "core/diagnostics/SystemCpuSampler.h"

#include <QObject>
#include <atomic>
#include <functional>

class CpuMonitorWorker : public QObject {
    Q_OBJECT
public:
    explicit CpuMonitorWorker(QObject* parent = nullptr);
    ~CpuMonitorWorker() override;

    void setFeatureRunningCallback(std::function<bool()> callback);

public slots:
    void startMonitoring(int intervalMs, int topN, const CpuSpikeDetectorConfig& detectorConfig);
    void stopMonitoring();
    void requestStop();

signals:
    void sampleReady(CpuSampleSnapshot snapshot);
    void spikeDetected(CpuSpikeEvent event);
    void monitoringStopped();

private:
    void runSampleLoop(int intervalMs, int topN);

    SystemCpuSampler m_systemSampler;
    ProcessCpuSampler m_processSampler;
    CpuSpikeDetector m_spikeDetector;
    std::function<bool()> m_featureRunningCallback;
    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_running{false};
};
