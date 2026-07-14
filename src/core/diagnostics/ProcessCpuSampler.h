#pragma once

#include "core/diagnostics/CpuMonitorTypes.h"

#include <QHash>
#include <QString>
#include <cstdint>
#include <functional>

class ProcessCpuSampler {
public:
    explicit ProcessCpuSampler(int logicalProcessorCount);

    QVector<ProcessCpuEntry> sampleTopProcesses(
        int topN,
        const std::function<bool()>& shouldAbort = {});

private:
    struct ProcessTimingState {
        std::uint64_t lastKernelUser = 0;
        int missedTicks = 0;
        QString name;
        QString executablePath;
    };

    static std::uint64_t fileTimeToUInt64(const void* fileTime);
    void resolveProcessIdentityFromHandle(void* processHandle,
                                          quint32 pid,
                                          const wchar_t* snapshotExeFile,
                                          QString& name,
                                          QString& executablePath);
    void resolveProcessIdentity(quint32 pid,
                                const wchar_t* snapshotExeFile,
                                QString& name,
                                QString& executablePath);

    int m_logicalProcessorCount = 1;
    std::uint64_t m_lastWallTime = 0;
    bool m_hasWallBaseline = false;
    QHash<quint32, ProcessTimingState> m_states;
};
