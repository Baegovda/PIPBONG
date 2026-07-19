#include "core/diagnostics/CrashReportSystemInfo.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QLocale>
#include <QScreen>
#include <QStringList>
#include <QThread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

namespace {

using RtlGetVersionFn = LONG(WINAPI*)(void*);

struct OsVersionNumbers {
    DWORD major = 0;
    DWORD minor = 0;
    DWORD build = 0;
};

OsVersionNumbers queryOsVersion() {
    OsVersionNumbers version;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return version;
    }
    const auto rtlGetVersion =
        reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) {
        return version;
    }

    struct RTL_OSVERSIONINFOW {
        ULONG dwOSVersionInfoSize;
        ULONG dwMajorVersion;
        ULONG dwMinorVersion;
        ULONG dwBuildNumber;
        ULONG dwPlatformId;
        WCHAR szCSDVersion[128];
    } info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion(&info) == 0) {
        version.major = info.dwMajorVersion;
        version.minor = info.dwMinorVersion;
        version.build = info.dwBuildNumber;
    }
    return version;
}

bool isProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) || !token) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok == TRUE && elevation.TokenIsElevated != 0;
}

QString formatFileTimeUtc(const FILETIME& fileTime) {
    SYSTEMTIME systemTime{};
    FILETIME localTime{};
    if (!FileTimeToLocalFileTime(&fileTime, &localTime)
        || !FileTimeToSystemTime(&localTime, &systemTime)) {
        return QStringLiteral("(unknown)");
    }
    return QStringLiteral("%1-%2-%3 %4:%5:%6")
        .arg(systemTime.wYear, 4, 10, QLatin1Char('0'))
        .arg(systemTime.wMonth, 2, 10, QLatin1Char('0'))
        .arg(systemTime.wDay, 2, 10, QLatin1Char('0'))
        .arg(systemTime.wHour, 2, 10, QLatin1Char('0'))
        .arg(systemTime.wMinute, 2, 10, QLatin1Char('0'))
        .arg(systemTime.wSecond, 2, 10, QLatin1Char('0'));
}

} // namespace
#endif

namespace CrashReportSystemInfo {

QString buildSystemSection() {
    QStringList lines;
    lines.append(QStringLiteral("--- system ---"));

#ifdef _WIN32
    const OsVersionNumbers osVersion = queryOsVersion();
    if (osVersion.major > 0) {
        lines.append(QStringLiteral("  OS: Windows %1.%2 build %3")
                         .arg(osVersion.major)
                         .arg(osVersion.minor)
                         .arg(osVersion.build));
    }

    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    lines.append(QStringLiteral("  processors: %1 (logical)").arg(systemInfo.dwNumberOfProcessors));
    lines.append(QStringLiteral("  elevated: %1").arg(isProcessElevated() ? QStringLiteral("yes")
                                                                          : QStringLiteral("no")));

    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        const quint64 totalMb = memoryStatus.ullTotalPhys / (1024ULL * 1024ULL);
        const quint64 availMb = memoryStatus.ullAvailPhys / (1024ULL * 1024ULL);
        lines.append(QStringLiteral("  physical memory: %1 MB total, %2 MB available (%3% used)")
                         .arg(totalMb)
                         .arg(availMb)
                         .arg(memoryStatus.dwMemoryLoad));
    }

    PROCESS_MEMORY_COUNTERS_EX processMemory{};
    processMemory.cb = sizeof(processMemory);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemory),
                             sizeof(processMemory))) {
        const quint64 workingSetMb = processMemory.WorkingSetSize / (1024ULL * 1024ULL);
        const quint64 privateMb = processMemory.PrivateUsage / (1024ULL * 1024ULL);
        lines.append(QStringLiteral("  process memory: working set %1 MB, private %2 MB")
                         .arg(workingSetMb)
                         .arg(privateMb));
    }

    FILETIME creationTime{};
    FILETIME exitTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        lines.append(QStringLiteral("  process started: %1").arg(formatFileTimeUtc(creationTime)));
        const ULONGLONG kernelMs =
            (static_cast<ULONGLONG>(kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        const ULONGLONG userMs =
            (static_cast<ULONGLONG>(userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
        lines.append(QStringLiteral("  process CPU time: kernel %1 ms, user %2 ms")
                         .arg(kernelMs / 10000ULL)
                         .arg(userMs / 10000ULL));
    }
#endif

    if (QCoreApplication::instance()) {
        lines.append(QStringLiteral("  Qt: %1").arg(QString::fromLatin1(qVersion())));
        lines.append(QStringLiteral("  locale: %1").arg(QLocale::system().name()));
        lines.append(QStringLiteral("  thread: %1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()),
                                                        0,
                                                        16));
    }

    if (const QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        const QList<QScreen*> screens = guiApp->screens();
        lines.append(QStringLiteral("  screens: %1").arg(screens.size()));
        for (int index = 0; index < screens.size(); ++index) {
            const QScreen* screen = screens.at(index);
            const QRect geometry = screen->geometry();
            lines.append(QStringLiteral("    [%1] %2x%3 @%4,%5 dpi=%6 primary=%7")
                             .arg(index)
                             .arg(geometry.width())
                             .arg(geometry.height())
                             .arg(geometry.x())
                             .arg(geometry.y())
                             .arg(screen->devicePixelRatio(), 0, 'f', 2)
                             .arg(screen == guiApp->primaryScreen() ? QStringLiteral("yes")
                                                                    : QStringLiteral("no")));
        }
    }

    return lines.join(QLatin1Char('\n'));
}

QString buildForegroundWindowSection() {
    QStringList lines;
    lines.append(QStringLiteral("--- foreground window ---"));

#ifdef _WIN32
    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        lines.append(QStringLiteral("  (none)"));
        return lines.join(QLatin1Char('\n'));
    }

    wchar_t titleBuffer[512]{};
    const int titleLength = GetWindowTextW(foreground, titleBuffer, 512);
    const QString title = titleLength > 0 ? QString::fromWCharArray(titleBuffer, titleLength)
                                          : QStringLiteral("(no title)");

    wchar_t classBuffer[256]{};
    const int classLength = GetClassNameW(foreground, classBuffer, 256);
    const QString className = classLength > 0 ? QString::fromWCharArray(classBuffer, classLength)
                                              : QStringLiteral("(unknown)");

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);

    wchar_t processPath[MAX_PATH]{};
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (processHandle) {
        DWORD pathLength = MAX_PATH;
        QueryFullProcessImageNameW(processHandle, 0, processPath, &pathLength);
        CloseHandle(processHandle);
    }

    RECT bounds{};
    const bool hasBounds = GetWindowRect(foreground, &bounds) == TRUE;
    lines.append(QStringLiteral("  hwnd: 0x%1").arg(reinterpret_cast<quintptr>(foreground), 16, 16, QLatin1Char('0')));
    lines.append(QStringLiteral("  title: %1").arg(title));
    lines.append(QStringLiteral("  class: %1").arg(className));
    lines.append(QStringLiteral("  processId: %1").arg(processId));
    if (processPath[0] != L'\0') {
        lines.append(QStringLiteral("  process: %1").arg(QString::fromWCharArray(processPath)));
    }
    if (hasBounds) {
        lines.append(QStringLiteral("  bounds: %1,%2 %3x%4")
                         .arg(bounds.left)
                         .arg(bounds.top)
                         .arg(bounds.right - bounds.left)
                         .arg(bounds.bottom - bounds.top));
    }
    lines.append(QStringLiteral("  visible: %1 minimized: %2")
                     .arg(IsWindowVisible(foreground) ? QStringLiteral("yes") : QStringLiteral("no"))
                     .arg(IsIconic(foreground) ? QStringLiteral("yes") : QStringLiteral("no")));
#else
    lines.append(QStringLiteral("  (unavailable on this platform)"));
#endif

    return lines.join(QLatin1Char('\n'));
}

} // namespace CrashReportSystemInfo
