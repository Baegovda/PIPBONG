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

void CpuMonitorWorker::requestStop() {
    m_stopRequested.store(true);
}

void CpuMonitorWorker::interruptibleSleep(int totalMs) {
    int remaining = totalMs;
    while (remaining > 0 && !m_stopRequested.load()) {
        const int chunk = std::min(remaining, 50);
        QThread::msleep(static_cast<unsigned long>(chunk));
        remaining -= chunk;
    }
}

void CpuMonitorWorker::runSampleLoop(int intervalMs, int topN) {
    const int sleepMs = std::max(200, intervalMs);
    const auto shouldAbort = [this]() { return m_stopRequested.load(); };

    while (!m_stopRequested.load()) {
        CpuSampleSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTime();

        const double systemCpu = m_systemSampler.sampleCpuPercent();
        snapshot.systemReady = systemCpu >= 0.0;
        snapshot.systemCpuPercent = snapshot.systemReady ? systemCpu : 0.0;
        snapshot.topProcesses = m_processSampler.sampleTopProcesses(topN, shouldAbort);

        if (m_stopRequested.load()) {
            break;
        }

        emit sampleReady(snapshot);

        if (snapshot.systemReady) {
            const bool featureRunning =
                m_featureRunningCallback ? m_featureRunningCallback() : false;
            if (const auto spike = m_spikeDetector.evaluate(snapshot, featureRunning)) {
                emit spikeDetected(*spike);
            }
        }

        interruptibleSleep(sleepMs);
    }

    m_running.store(false);
    m_stopRequested.store(false);
    emit monitoringStopped();
}
