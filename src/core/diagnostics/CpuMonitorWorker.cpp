#include "core/diagnostics/CpuMonitorWorker.h"

#include <QThread>
#include <algorithm>

CpuMonitorWorker::CpuMonitorWorker(QObject* parent)
    : QObject(parent)
    , m_processSampler(m_systemSampler.logicalProcessorCount()) {}

CpuMonitorWorker::~CpuMonitorWorker() {
    requestStop();
}

void CpuMonitorWorker::setFeatureRunningCallback(std::function<bool()> callback) {
    m_featureRunningCallback = std::move(callback);
}

void CpuMonitorWorker::startMonitoring(int intervalMs,
                                       int topN,
                                       const CpuSpikeDetectorConfig& detectorConfig) {
    if (m_running.exchange(true)) {
        return;
    }
    m_stopRequested.store(false);
    m_spikeDetector.setConfig(detectorConfig);
    runSampleLoop(intervalMs, topN);
}

void CpuMonitorWorker::stopMonitoring() {
    requestStop();
}

void CpuMonitorWorker::requestStop() {
    m_stopRequested.store(true);
}

void CpuMonitorWorker::runSampleLoop(int intervalMs, int topN) {
    const int sleepMs = std::max(200, intervalMs);

    while (!m_stopRequested.load()) {
        CpuSampleSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTime();

        const double systemCpu = m_systemSampler.sampleCpuPercent();
        snapshot.systemReady = systemCpu >= 0.0;
        snapshot.systemCpuPercent = snapshot.systemReady ? systemCpu : 0.0;
        snapshot.topProcesses = m_processSampler.sampleTopProcesses(topN);

        emit sampleReady(snapshot);

        if (snapshot.systemReady) {
            const bool featureRunning =
                m_featureRunningCallback ? m_featureRunningCallback() : false;
            if (const auto spike = m_spikeDetector.evaluate(snapshot, featureRunning)) {
                emit spikeDetected(*spike);
            }
        }

        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }

    m_running.store(false);
    emit monitoringStopped();
}
