#pragma once

#include <cstdint>

class SystemCpuSampler {
public:
    SystemCpuSampler();

    /// Returns system-wide CPU usage 0–100, or -1 on the first baseline tick.
    double sampleCpuPercent();

    int logicalProcessorCount() const { return m_logicalProcessorCount; }

private:
    static std::uint64_t fileTimeToUInt64(const void* fileTime);

    bool m_hasBaseline = false;
    std::uint64_t m_lastIdle = 0;
    std::uint64_t m_lastKernel = 0;
    std::uint64_t m_lastUser = 0;
    int m_logicalProcessorCount = 1;
};
