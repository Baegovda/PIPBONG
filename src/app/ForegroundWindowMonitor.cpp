#include "ForegroundWindowMonitor.h"

#include "core/capture/ScreenCapture.h"
#include "core/diagnostics/CrashReporter.h"

#include <QDateTime>
#include <QMetaObject>
#include <QTimer>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace {

#ifdef _WIN32
ForegroundWindowMonitor* g_activeForegroundMonitor = nullptr;
#endif

} // namespace

ForegroundWindowMonitor::ForegroundWindowMonitor(QObject* parent)
    : QObject(parent) {
#ifdef _WIN32
    m_coalesceTimer = new QTimer(this);
    m_coalesceTimer->setSingleShot(true);
    connect(m_coalesceTimer, &QTimer::timeout, this, [this]() {
        m_pendingEmit = false;
        emit foregroundChanged(m_state);
    });
#endif
}

ForegroundWindowMonitor::~ForegroundWindowMonitor() {
    stop();
}

void ForegroundWindowMonitor::start() {
#ifdef _WIN32
    if (m_started) {
        return;
    }
    m_started = true;
    g_activeForegroundMonitor = this;
    m_altTabModifierWasHeld = isAltTabModifierHeld();
    m_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                             EVENT_SYSTEM_FOREGROUND,
                             nullptr,
                             &ForegroundWindowMonitor::winEventProc,
                             0,
                             0,
                             WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    refreshFromForegroundHwnd(GetForegroundWindow(), true);
#endif
}

bool ForegroundWindowMonitor::syncFromDesktopForeground() {
#ifdef _WIN32
    if (!m_started) {
        return false;
    }
    const ForegroundWindowState before = m_state;
    refreshFromForegroundHwnd(GetForegroundWindow(), false);
    return !before.isEquivalentTo(m_state);
#else
    return false;
#endif
}

void ForegroundWindowMonitor::stop() {
#ifdef _WIN32
    if (!m_started) {
        return;
    }
    m_started = false;
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
    if (g_activeForegroundMonitor == this) {
        g_activeForegroundMonitor = nullptr;
    }
    if (m_coalesceTimer) {
        m_coalesceTimer->stop();
    }
    m_pendingEmit = false;
#endif
}

#ifdef _WIN32
bool ForegroundWindowMonitor::isShellTransientWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return true;
    }
    wchar_t className[256]{};
    if (GetClassNameW(hwnd, className, 256) <= 0) {
        return false;
    }
    static const wchar_t* kIgnoredClasses[] = {
        L"#32771",
        L"ForegroundStaging",
        L"MultitaskingViewFrame",
        L"XamlExplorerHostIslandWindow",
        L"TaskSwitcherWnd",
        L"Xaml_Window",
        L"Windows.UI.Core.CoreWindow",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow",
        L"WorkerW",
        L"Progman",
    };
    for (const wchar_t* ignored : kIgnoredClasses) {
        if (_wcsicmp(className, ignored) == 0) {
            return true;
        }
    }
    return false;
}

bool ForegroundWindowMonitor::isAltTabModifierHeld() {
    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0
           || (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
}

bool ForegroundWindowMonitor::isPipbongProcessWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

ForegroundWindowState ForegroundWindowMonitor::buildStateFromHwnd(HWND foregroundHwnd) {
    ForegroundWindowState state;
    if (!foregroundHwnd || !IsWindow(foregroundHwnd)) {
        return state;
    }
    state.pipbong = isPipbongProcessWindow(foregroundHwnd);
    state.rootHwnd = GetAncestor(foregroundHwnd, GA_ROOT);
    if (!state.rootHwnd || !IsWindow(state.rootHwnd)) {
        state.rootHwnd = nullptr;
        return state;
    }
    state.shellTransient = isShellTransientWindow(state.rootHwnd);
    wchar_t titleBuffer[512]{};
    GetWindowTextW(state.rootHwnd, titleBuffer, 512);
    state.title = QString::fromWCharArray(titleBuffer).trimmed();

    ScreenCapture::TargetWindowInfo info;
    if (ScreenCapture::queryWindowInfo(state.rootHwnd, info) && !info.processPath.empty()) {
        state.processPath = QString::fromStdWString(info.processPath);
    }
    return state;
}

void CALLBACK ForegroundWindowMonitor::winEventProc(HWINEVENTHOOK,
                                                  DWORD event,
                                                  HWND,
                                                  LONG,
                                                  LONG,
                                                  DWORD,
                                                  DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND || !g_activeForegroundMonitor) {
        return;
    }
    QMetaObject::invokeMethod(g_activeForegroundMonitor,
                              "onWinEvent",
                              Qt::QueuedConnection);
}

void ForegroundWindowMonitor::onWinEvent() {
    const bool altHeld = isAltTabModifierHeld();
    if (m_altTabModifierWasHeld && !altHeld) {
        emit altModifierReleased();
    }
    m_altTabModifierWasHeld = altHeld;
    refreshFromForegroundHwnd(GetForegroundWindow(), false);
}

void ForegroundWindowMonitor::refreshFromForegroundHwnd(HWND foregroundHwnd, bool forceEmit) {
    ForegroundWindowState next = buildStateFromHwnd(foregroundHwnd);
    if (!forceEmit && next.isEquivalentTo(m_state)) {
        return;
    }
    next.monotonicSeq = m_nextSeq++;
    next.changedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_state = next;

    CrashReporter::noteBreadcrumb(
        QStringLiteral("foreground"),
        QStringLiteral("hwnd=%1 title=%2")
            .arg(reinterpret_cast<quintptr>(m_state.rootHwnd), 0, 16)
            .arg(m_state.title.left(48)));

    if (forceEmit) {
        emit foregroundChanged(m_state);
        return;
    }
    if (!m_pendingEmit) {
        m_pendingEmit = true;
        m_coalesceTimer->start(0);
    }
}

#endif
