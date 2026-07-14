#include "core/diagnostics/SystemCpuSampler.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <thread>

SystemCpuSampler::SystemCpuSampler() {
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    m_logicalProcessorCount = hw > 0 ? hw : 1;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0) {
        m_logicalProcessorCount = static_cast<int>(info.dwNumberOfProcessors);
    }
#endif
}

std::uint64_t SystemCpuSampler::fileTimeToUInt64(const void* fileTime) {
#ifdef _WIN32
    const auto* ft = static_cast<const FILETIME*>(fileTime);
    ULARGE_INTEGER ui{};
    ui.LowPart = ft->dwLowDateTime;
    ui.HighPart = ft->dwHighDateTime;
    return ui.QuadPart;
#else
    (void)fileTime;
    return 0;
#endif
}

double SystemCpuSampler::sampleCpuPercent() {
#ifdef _WIN32
    FILETIME idleFt{};
    FILETIME kernelFt{};
    FILETIME userFt{};
    if (!GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
        return -1.0;
    }

    const std::uint64_t idle = fileTimeToUInt64(&idleFt);
    const std::uint64_t kernel = fileTimeToUInt64(&kernelFt);
    const std::uint64_t user = fileTimeToUInt64(&userFt);

    if (!m_hasBaseline) {
        m_lastIdle = idle;
        m_lastKernel = kernel;
        m_lastUser = user;
        m_hasBaseline = true;
        return -1.0;
    }

    const std::uint64_t idleDelta = idle - m_lastIdle;
    const std::uint64_t kernelDelta = kernel - m_lastKernel;
    const std::uint64_t userDelta = user - m_lastUser;
    m_lastIdle = idle;
    m_lastKernel = kernel;
    m_lastUser = user;

    const std::uint64_t totalDelta = kernelDelta + userDelta;
    if (totalDelta == 0) {
        return 0.0;
    }

    const double busy = static_cast<double>(totalDelta - idleDelta);
    const double usage = (busy / static_cast<double>(totalDelta)) * 100.0;
    if (usage < 0.0) {
        return 0.0;
    }
    if (usage > 100.0) {
        return 100.0;
    }
    return usage;
#else
    return -1.0;
#endif
}
