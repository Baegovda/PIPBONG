#pragma once

#include "core/diagnostics/CpuMonitorTypes.h"

#include <QHash>
#include <QString>
#include <cstdint>

class ProcessCpuSampler {
public:
    explicit ProcessCpuSampler(int logicalProcessorCount);

    QVector<ProcessCpuEntry> sampleTopProcesses(int topN);

private:
    struct ProcessTimingState {
        std::uint64_t lastKernelUser = 0;
        int missedTicks = 0;
        QString name;
    };

    static std::uint64_t fileTimeToUInt64(const void* fileTime);
    QString resolveProcessName(quint32 pid, const wchar_t* snapshotExeFile);

    int m_logicalProcessorCount = 1;
    std::uint64_t m_lastWallTime = 0;
    bool m_hasWallBaseline = false;
    QHash<quint32, ProcessTimingState> m_states;
};
