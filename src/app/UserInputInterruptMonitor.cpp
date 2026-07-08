#include "app/UserInputInterruptMonitor.h"

#include "app/FeatureHotkeyGate.h"
#include "core/input/HotkeyBinding.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QMetaObject>

#include <vector>

namespace {

#ifdef _WIN32
UserInputInterruptMonitor* g_userInputInterruptMonitor = nullptr;
HHOOK g_keyboardHook = nullptr;
HHOOK g_mouseHook = nullptr;

bool isMouseOverOwnProcessWindow(const MSLLHOOKSTRUCT* info) {
    if (!info) {
        return false;
    }
    HWND hwnd = WindowFromPoint(info->pt);
    if (!hwnd) {
        return false;
    }
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    return windowPid == GetCurrentProcessId();
}

bool isModifierOnlyVirtualKey(int vk) {
    switch (vk) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

bool keyboardModifiersMatch(const HotkeyBinding& hotkey) {
    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    return hotkey.ctrl == ctrlDown && hotkey.alt == altDown && hotkey.shift == shiftDown;
}

bool matchesFeatureHotkey(const HotkeyBinding& hotkey, int vk) {
    if (hotkey.isEmpty() || hotkey.virtualKey != vk) {
        return false;
    }
    if (HotkeyBinding::isMouseVirtualKey(vk)) {
        return true;
    }
    return keyboardModifiersMatch(hotkey);
}

LRESULT CALLBACK interruptKeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_userInputInterruptMonitor) {
        return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
    }

    const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    if (!keyDown) {
        return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (info->flags & LLKHF_INJECTED) {
        return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
    }
    if (info->flags & 0x40000000) { // LLKHF_REPEAT — key held, not initial press
        return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
    }

    const int vk = static_cast<int>(info->vkCode);
    if (isModifierOnlyVirtualKey(vk)) {
        return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
    }

    g_userInputInterruptMonitor->notifyPhysicalInput(vk);
    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

int virtualKeyFromMouseMessage(WPARAM wParam, const MSLLHOOKSTRUCT* info) {
    switch (wParam) {
    case WM_LBUTTONDOWN:
        return VK_LBUTTON;
    case WM_RBUTTONDOWN:
        return VK_RBUTTON;
    case WM_MBUTTONDOWN:
        return VK_MBUTTON;
    case WM_XBUTTONDOWN:
        if (!info) {
            return 0;
        }
        return HIWORD(info->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
    default:
        return 0;
    }
}

LRESULT CALLBACK interruptMouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_userInputInterruptMonitor) {
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (info->flags & LLMHF_INJECTED) {
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }
    if (isMouseOverOwnProcessWindow(info)) {
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }

    const int vk = virtualKeyFromMouseMessage(wParam, info);
    if (vk == 0) {
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }

    g_userInputInterruptMonitor->notifyPhysicalInput(vk);
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}
#endif

constexpr auto kInterruptCooldown = std::chrono::milliseconds(300);

} // namespace

UserInputInterruptMonitor& UserInputInterruptMonitor::instance() {
    static UserInputInterruptMonitor monitor;
    return monitor;
}

void UserInputInterruptMonitor::setHandler(InterruptHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = std::move(handler);
}

void UserInputInterruptMonitor::setHotkeyExemptionCheck(std::function<bool(int virtualKey)> check) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hotkeyExemptionCheck = std::move(check);
}

void UserInputInterruptMonitor::registerSession(const std::string& featureId,
                                                UserInputInterruptMode mode,
                                                const HotkeyBinding& featureHotkey,
                                                ExecutionContext* context) {
    if (featureId.empty() || !context) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        SessionEntry entry;
        entry.mode = mode;
        entry.featureHotkey = featureHotkey;
        entry.context = context;
        m_sessions[featureId] = entry;
    }
    refreshHooks();
}

void UserInputInterruptMonitor::unregisterSession(const std::string& featureId) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.erase(featureId);
    }
    refreshHooks();
}

void UserInputInterruptMonitor::unregisterAll() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.clear();
    }
    refreshHooks();
}

void UserInputInterruptMonitor::refreshHooks() {
#ifdef _WIN32
    bool needHooks = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        needHooks = !m_sessions.empty();
    }
    if (needHooks && !g_keyboardHook) {
        g_userInputInterruptMonitor = this;
        g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, interruptKeyboardHookProc, nullptr, 0);
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, interruptMouseHookProc, nullptr, 0);
    } else if (!needHooks) {
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }
        if (g_mouseHook) {
            UnhookWindowsHookEx(g_mouseHook);
            g_mouseHook = nullptr;
        }
        if (g_userInputInterruptMonitor == this) {
            g_userInputInterruptMonitor = nullptr;
        }
    }
#endif
}

void UserInputInterruptMonitor::notifyPhysicalInput(int virtualKey) {
    if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
        return;
    }

    std::vector<std::string> targets;
    InterruptHandler handler;
    std::function<bool(int)> hotkeyExemptionCheck;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handler = m_handler;
        hotkeyExemptionCheck = m_hotkeyExemptionCheck;

        if (hotkeyExemptionCheck && hotkeyExemptionCheck(virtualKey)) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        for (auto& [featureId, entry] : m_sessions) {
            if (!entry.context) {
                continue;
            }
            if (now - entry.lastInterruptAt < kInterruptCooldown) {
                continue;
            }
            entry.lastInterruptAt = now;
            targets.push_back(featureId);
        }
    }

    if (!handler || targets.empty()) {
        return;
    }

    for (const std::string& featureId : targets) {
        const std::string capturedId = featureId;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [handler, capturedId]() { handler(capturedId); },
            Qt::QueuedConnection);
    }
}
