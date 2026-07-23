#include "app/UserInputInterruptMonitor.h"

#include "app/FeatureHotkeyGate.h"
#include "core/diagnostics/WorkflowRunProfiler.h"
#include "core/input/HotkeyBinding.h"
#include "core/workflow/ExecutionContext.h"

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

QTimer* inputPollTimer() {
    static QTimer* timer = nullptr;
    if (!timer && QCoreApplication::instance()) {
        timer = new QTimer(QCoreApplication::instance());
        timer->setInterval(32);
        QObject::connect(timer, &QTimer::timeout, timer, []() {
            if (g_userInputInterruptMonitor) {
                g_userInputInterruptMonitor->pollInputEdges();
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
                                                ExecutionContext* context,
                                                bool keyboardInterrupt) {
    if (featureId.empty() || !context) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        SessionEntry entry;
        entry.mode = mode;
        entry.featureHotkey = featureHotkey;
        entry.context = context;
        entry.keyboardInterrupt = keyboardInterrupt;
        const auto existing = m_sessions.find(featureId);
        if (existing != m_sessions.end()) {
            entry.lastInterruptAt = existing->second.lastInterruptAt;
        }
        m_sessions[featureId] = entry;
    }
    refreshPollTimer();
}

void UserInputInterruptMonitor::unregisterSession(const std::string& featureId) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.erase(featureId);
    }
    refreshPollTimer();
}

void UserInputInterruptMonitor::unregisterAll() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.clear();
        m_mouseButtonWasDown.fill(false);
        m_keyboardWasDown.fill(false);
        m_keyboardPollEnabled = false;
    }
    refreshPollTimer();
}

void UserInputInterruptMonitor::refreshPollTimer() {
#ifdef _WIN32
    bool needPoll = false;
    bool keyboardPoll = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        needPoll = !m_sessions.empty();
        for (const auto& [featureId, entry] : m_sessions) {
            (void)featureId;
            if (entry.keyboardInterrupt) {
                keyboardPoll = true;
                break;
            }
        }
        m_keyboardPollEnabled = keyboardPoll;
    }

    if (needPoll) {
        g_userInputInterruptMonitor = this;
        if (QTimer* timer = inputPollTimer()) {
            timer->start();
        }
    } else {
        if (QTimer* timer = inputPollTimer()) {
            timer->stop();
        }
        if (g_userInputInterruptMonitor == this) {
            g_userInputInterruptMonitor = nullptr;
        }
    }
#endif
}

void UserInputInterruptMonitor::pollInputEdges() {
#ifdef _WIN32
    pollMouseButtonEdges();
    if (m_keyboardPollEnabled) {
        pollKeyboardEdges();
    }
#else
    (void)0;
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
            if (!shouldSuppressUserInputInterruptOnAnySession(button.vk)) {
                notifyPhysicalInput(button.vk);
            }
        }
    }
#else
    (void)0;
#endif
}

bool UserInputInterruptMonitor::shouldSuppressUserInputInterruptOnAnySession(int virtualKey) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [featureId, entry] : m_sessions) {
        (void)featureId;
        if (entry.context && entry.context->shouldSuppressUserInputInterrupt(virtualKey)) {
            return true;
        }
    }
    return false;
}

void UserInputInterruptMonitor::pollKeyboardEdges() {
#ifdef _WIN32
    if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
        return;
    }

    BYTE keyboardState[256] = {};
    if (!GetKeyboardState(keyboardState)) {
        return;
    }

    std::function<bool(int)> hotkeyExemptionCheck;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        hotkeyExemptionCheck = m_hotkeyExemptionCheck;
    }

    for (int vk = 0x08; vk < 256; ++vk) {
        if (isModifierOnlyVirtualKey(vk)) {
            continue;
        }

        const bool down = (keyboardState[vk] & 0x80) != 0;
        bool wasDown = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            wasDown = m_keyboardWasDown[static_cast<size_t>(vk)];
            m_keyboardWasDown[static_cast<size_t>(vk)] = down;
        }
        if (!down || wasDown) {
            continue;
        }
        if (hotkeyExemptionCheck && hotkeyExemptionCheck(vk)) {
            continue;
        }
        if (shouldSuppressUserInputInterruptOnAnySession(vk)) {
            continue;
        }
        notifyPhysicalInput(vk);
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
        WorkflowRunProfiler::recordUserInputInterrupt(QString::fromStdString(featureId), virtualKey);
        const std::string capturedId = featureId;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [handler, capturedId]() { handler(capturedId); },
            Qt::QueuedConnection);
    }
}
