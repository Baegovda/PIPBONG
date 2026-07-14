#include "core/diagnostics/ProcessCpuSampler.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

#include <algorithm>
#include <vector>

namespace {

constexpr int kPruneAfterMissedTicks = 10;

} // namespace

ProcessCpuSampler::ProcessCpuSampler(int logicalProcessorCount)
    : m_logicalProcessorCount(logicalProcessorCount > 0 ? logicalProcessorCount : 1) {}

std::uint64_t ProcessCpuSampler::fileTimeToUInt64(const void* fileTime) {
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

QString ProcessCpuSampler::resolveProcessName(quint32 pid, const wchar_t* snapshotExeFile) {
#ifdef _WIN32
    if (snapshotExeFile && snapshotExeFile[0] != L'\0') {
        return QString::fromWCharArray(snapshotExeFile);
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return QStringLiteral("PID %1").arg(pid);
    }

    wchar_t buffer[MAX_PATH]{};
    DWORD size = MAX_PATH;
    QString name;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        const QString fullPath = QString::fromWCharArray(buffer);
        const int slash = fullPath.lastIndexOf(QLatin1Char('\\'));
        name = slash >= 0 ? fullPath.mid(slash + 1) : fullPath;
    } else {
        name = QStringLiteral("PID %1").arg(pid);
    }
    CloseHandle(process);
    return name;
#else
    (void)pid;
    (void)snapshotExeFile;
    return QString();
#endif
}

QVector<ProcessCpuEntry> ProcessCpuSampler::sampleTopProcesses(int topN) {
    QVector<ProcessCpuEntry> result;
#ifdef _WIN32
    if (topN <= 0) {
        return result;
    }

    FILETIME wallFt{};
    GetSystemTimeAsFileTime(&wallFt);
    const std::uint64_t wallTime = fileTimeToUInt64(&wallFt);
    if (!m_hasWallBaseline) {
        m_lastWallTime = wallTime;
        m_hasWallBaseline = true;
        return result;
    }

    const std::uint64_t wallDelta = wallTime - m_lastWallTime;
    m_lastWallTime = wallTime;
    if (wallDelta == 0) {
        return result;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    QHash<quint32, ProcessTimingState> nextStates;
    std::vector<ProcessCpuEntry> entries;
    entries.reserve(128);

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snapshot, &pe)) {
        do {
            const quint32 pid = pe.th32ProcessID;
            if (pid == 0) {
                continue;
            }

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!process) {
                continue;
            }

            FILETIME createFt{};
            FILETIME exitFt{};
            FILETIME kernelFt{};
            FILETIME userFt{};
            if (!GetProcessTimes(process, &createFt, &exitFt, &kernelFt, &userFt)) {
                CloseHandle(process);
                continue;
            }
            CloseHandle(process);

            const std::uint64_t procTime = fileTimeToUInt64(&kernelFt) + fileTimeToUInt64(&userFt);
            ProcessTimingState state = m_states.value(pid);
            if (state.name.isEmpty()) {
                state.name = resolveProcessName(pid, pe.szExeFile);
            }

            double cpuPercent = 0.0;
            if (m_states.contains(pid) && state.lastKernelUser > 0) {
                const std::uint64_t procDelta = procTime - state.lastKernelUser;
                cpuPercent = (static_cast<double>(procDelta) / static_cast<double>(wallDelta))
                             * 100.0 / static_cast<double>(m_logicalProcessorCount);
            }

            state.lastKernelUser = procTime;
            state.missedTicks = 0;
            nextStates.insert(pid, state);

            if (cpuPercent > 0.01) {
                ProcessCpuEntry entry;
                entry.pid = pid;
                entry.name = state.name;
                entry.cpuPercent = cpuPercent;
                entry.accessible = true;
                entries.push_back(entry);
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);

    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (nextStates.contains(it.key())) {
            continue;
        }
        ProcessTimingState stale = it.value();
        ++stale.missedTicks;
        if (stale.missedTicks <= kPruneAfterMissedTicks) {
            nextStates.insert(it.key(), stale);
        }
    }
    m_states = std::move(nextStates);

    std::sort(entries.begin(), entries.end(), [](const ProcessCpuEntry& a, const ProcessCpuEntry& b) {
        return a.cpuPercent > b.cpuPercent;
    });

    const int count = std::min(topN, static_cast<int>(entries.size()));
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(entries[static_cast<size_t>(i)]);
    }
#else
    (void)topN;
#endif
    return result;
}
