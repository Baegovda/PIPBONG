#include "app/UserInputInterruptMonitor.h"

#include "app/FeatureHotkeyGate.h"
#include "core/diagnostics/WorkflowRunProfiler.h"
#include "core/input/HotkeyBinding.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QMetaObject>
#include <QTimer>

#include <vector>

namespace {

#ifdef _WIN32
UserInputInterruptMonitor* g_userInputInterruptMonitor = nullptr;
HHOOK g_keyboardHook = nullptr;

QTimer* mouseButtonPollTimer() {
    static QTimer* timer = nullptr;
    if (!timer && QCoreApplication::instance()) {
        timer = new QTimer(QCoreApplication::instance());
        timer->setInterval(32);
        QObject::connect(timer, &QTimer::timeout, timer, []() {
            if (g_userInputInterruptMonitor) {
                g_userInputInterruptMonitor->pollMouseButtonEdges();
            }
        });
    }
    return timer;
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
        const auto existing = m_sessions.find(featureId);
        if (existing != m_sessions.end()) {
            entry.lastInterruptAt = existing->second.lastInterruptAt;
        }
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
        m_mouseButtonWasDown.fill(false);
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
        if (QTimer* timer = mouseButtonPollTimer()) {
            timer->start();
        }
    } else if (!needHooks) {
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }
        if (QTimer* timer = mouseButtonPollTimer()) {
            timer->stop();
        }
        if (g_userInputInterruptMonitor == this) {
            g_userInputInterruptMonitor = nullptr;
        }
    }
#endif
}

void UserInputInterruptMonitor::pollMouseButtonEdges() {
#ifdef _WIN32
    if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
        return;
    }

    struct MouseButton {
        int vk;
        size_t index;
    };
    static constexpr MouseButton kButtons[] = {
        {VK_LBUTTON, 0},
        {VK_RBUTTON, 1},
        {VK_MBUTTON, 2},
        {VK_XBUTTON1, 3},
        {VK_XBUTTON2, 4},
    };

    for (const MouseButton& button : kButtons) {
        const bool down = (GetAsyncKeyState(button.vk) & 0x8000) != 0;
        bool wasDown = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            wasDown = m_mouseButtonWasDown[button.index];
            m_mouseButtonWasDown[button.index] = down;
        }
        if (down && !wasDown) {
            notifyPhysicalInput(button.vk);
        }
    }
#else
    (void)0;
#endif
}

void UserInputInterruptMonitor::notifyPhysicalInput(int virtualKey) {
    if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
        return;
    }

    if (HotkeyBinding::isMouseVirtualKey(virtualKey)) {
        WorkflowRunProfiler::recordPhysicalInput("user_physical_mouse", virtualKey);
    } else {
        WorkflowRunProfiler::recordPhysicalInput("user_physical_key", virtualKey);
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
