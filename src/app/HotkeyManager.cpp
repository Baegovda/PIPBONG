#include "app/HotkeyManager.h"

#include "app/FeatureHotkeyGate.h"
#include "core/input/HotkeyBinding.h"
#include "model/Feature.h"
#include "model/FeatureRunMode.h"
#include "model/Project.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QMetaObject>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
namespace {

constexpr wchar_t kHotkeyHostClassName[] = L"PIPBONGHotkeyHost";

HotkeyManager* g_hotkeyManager = nullptr;

LRESULT CALLBACK hotkeyHostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND createHotkeyHostWindow() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = hotkeyHostWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kHotkeyHostClassName;
    RegisterClassW(&wc);

    return CreateWindowExW(0,
                           kHotkeyHostClassName,
                           L"",
                           0,
                           0,
                           0,
                           0,
                           0,
                           HWND_MESSAGE,
                           nullptr,
                           instance,
                           nullptr);
}

void restoreForegroundWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    if (GetForegroundWindow() == hwnd) {
        return;
    }

    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
}

int virtualKeyFromMouseMessage(WPARAM wParam, const MSLLHOOKSTRUCT* info) {
    switch (wParam) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        return VK_LBUTTON;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return VK_RBUTTON;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        return VK_MBUTTON;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        if (!info) {
            return 0;
        }
        return HIWORD(info->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
    default:
        return 0;
    }
}

LRESULT CALLBACK holdKeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_hotkeyManager) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool keyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!keyDown && !keyUp) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (info->flags & LLKHF_INJECTED) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    if (g_hotkeyManager->handleHoldKeyEvent(static_cast<int>(info->vkCode), keyDown)) {
        return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_hotkeyManager) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const bool buttonDown =
        wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN
        || wParam == WM_XBUTTONDOWN;
    const bool buttonUp = wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP || wParam == WM_MBUTTONUP
                          || wParam == WM_XBUTTONUP;
    if (!buttonDown && !buttonUp) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (info->flags & LLMHF_INJECTED) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    const int vk = virtualKeyFromMouseMessage(wParam, info);
    if (vk != 0 && g_hotkeyManager->handleMouseButtonEvent(vk, buttonDown)) {
        return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace
#endif

class HotkeyManager::NativeFilter : public QAbstractNativeEventFilter {
public:
    explicit NativeFilter(HotkeyManager* manager)
        : m_manager(manager) {}

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) override {
#ifdef _WIN32
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatch_MSG") {
            return false;
        }

        const auto* msg = static_cast<MSG*>(message);
        if (msg->message != WM_HOTKEY) {
            return false;
        }

        m_manager->handleHotkey(static_cast<int>(msg->wParam));
        return true;
#else
        (void)eventType;
        (void)message;
        return false;
#endif
    }

private:
    HotkeyManager* m_manager = nullptr;
};

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent)
    , m_filter(std::make_unique<NativeFilter>(this)) {
#ifdef _WIN32
    g_hotkeyManager = this;
    m_hotkeyHostHwnd = createHotkeyHostWindow();
#endif
    QCoreApplication::instance()->installNativeEventFilter(m_filter.get());
}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(m_filter.get());
    }
#ifdef _WIN32
    if (m_hotkeyHostHwnd) {
        DestroyWindow(static_cast<HWND>(m_hotkeyHostHwnd));
        m_hotkeyHostHwnd = nullptr;
    }
    if (g_hotkeyManager == this) {
        g_hotkeyManager = nullptr;
    }
#endif
}

void HotkeyManager::unregisterAll() {
#ifdef _WIN32
    uninstallHoldHook();
    uninstallMouseHook();
    const HWND host = static_cast<HWND>(m_hotkeyHostHwnd);
    for (const auto& entry : m_idToFeatureId) {
        UnregisterHotKey(host, entry.first);
    }
#endif
    m_idToFeatureId.clear();
    m_holdBindings.clear();
    m_mouseBindings.clear();
    m_nextId = 1;
}

std::vector<HotkeyManager::RegistrationFailure> HotkeyManager::syncFromProject(const Project& project) {
    unregisterAll();

    std::vector<RegistrationFailure> failures;

#ifdef _WIN32
    const HWND host = static_cast<HWND>(m_hotkeyHostHwnd);
    if (!host) {
        return failures;
    }

    for (const auto& feature : project.features()) {
        const HotkeyBinding& binding = feature->hotkey();
        if (binding.isEmpty()) {
            continue;
        }

        if (binding.isMouseButton()) {
            MouseBindingEntry entry;
            entry.featureId = feature->id();
            entry.binding = binding;
            entry.holdMode = feature->runMode() == FeatureRunMode::Hold;
            m_mouseBindings.push_back(entry);
            continue;
        }

        if (feature->runMode() == FeatureRunMode::Hold) {
            HoldBindingEntry entry;
            entry.featureId = feature->id();
            entry.binding = binding;
            m_holdBindings.push_back(entry);
            continue;
        }

        const int id = m_nextId++;
        if (RegisterHotKey(host, id, binding.winModifiers(), static_cast<UINT>(binding.virtualKey))) {
            m_idToFeatureId[id] = feature->id();
        } else {
            failures.push_back({feature->id(), feature->name()});
        }
    }

    if (!m_holdBindings.empty()) {
        installHoldHook();
    }
    if (!m_mouseBindings.empty()) {
        installMouseHook();
    }
#else
    (void)project;
#endif

    return failures;
}

bool HotkeyManager::isHoldBindingDown(const std::string& featureId) const {
    for (const auto& entry : m_holdBindings) {
        if (entry.featureId == featureId) {
            return entry.keyDown;
        }
    }
    for (const auto& entry : m_mouseBindings) {
        if (entry.holdMode && entry.featureId == featureId) {
            return entry.buttonDown;
        }
    }
    return false;
}

const Feature* HotkeyManager::findDuplicateHotkey(const Project& project,
                                                    const std::string& featureId,
                                                    const HotkeyBinding& binding) {
    if (binding.isEmpty()) {
        return nullptr;
    }

    for (const auto& feature : project.features()) {
        if (feature->id() == featureId) {
            continue;
        }
        if (!feature->hotkey().isEmpty() && feature->hotkey() == binding) {
            return feature.get();
        }
    }
    return nullptr;
}

void HotkeyManager::handleHotkey(int hotkeyId) {
    if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
        return;
    }

    const auto it = m_idToFeatureId.find(hotkeyId);
    if (it == m_idToFeatureId.end()) {
        return;
    }

#ifdef _WIN32
    const HWND previousForeground = GetForegroundWindow();
    const QString featureId = QString::fromStdString(it->second);

    QMetaObject::invokeMethod(
        this,
        [this, featureId, previousForeground]() {
            restoreForegroundWindow(previousForeground);
            emit hotkeyTriggered(featureId);
        },
        Qt::QueuedConnection);
#else
    emit hotkeyTriggered(QString::fromStdString(it->second));
#endif
}

#ifdef _WIN32
void HotkeyManager::installHoldHook() {
    if (m_holdHookInstalled) {
        return;
    }
    m_holdKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, holdKeyboardHookProc, GetModuleHandleW(nullptr), 0);
    m_holdHookInstalled = m_holdKeyboardHook != nullptr;
}

void HotkeyManager::uninstallHoldHook() {
    if (m_holdKeyboardHook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_holdKeyboardHook));
        m_holdKeyboardHook = nullptr;
    }
    m_holdHookInstalled = false;
    for (HoldBindingEntry& entry : m_holdBindings) {
        entry.keyDown = false;
    }
}

void HotkeyManager::installMouseHook() {
    if (m_mouseHookInstalled) {
        return;
    }
    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, GetModuleHandleW(nullptr), 0);
    m_mouseHookInstalled = m_mouseHook != nullptr;
}

void HotkeyManager::uninstallMouseHook() {
    if (m_mouseHook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_mouseHook));
        m_mouseHook = nullptr;
    }
    m_mouseHookInstalled = false;
    for (MouseBindingEntry& entry : m_mouseBindings) {
        entry.buttonDown = false;
    }
}

bool HotkeyManager::handleHoldKeyEvent(int vkCode, bool keyDown) {
    if (HotkeyBinding::isMouseVirtualKey(vkCode)) {
        return false;
    }

    bool swallow = false;
    for (HoldBindingEntry& entry : m_holdBindings) {
        if (!entry.binding.matchesVirtualKey(vkCode)) {
            continue;
        }
        if (!entry.binding.modifiersMatch() && keyDown) {
            continue;
        }
        if (keyDown) {
            if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
                continue;
            }
            if (entry.keyDown) {
                continue;
            }
            entry.keyDown = true;
            const QString featureId = QString::fromStdString(entry.featureId);
            const HWND previousForeground = GetForegroundWindow();
            QMetaObject::invokeMethod(
                this,
                [this, featureId, previousForeground]() {
                    restoreForegroundWindow(previousForeground);
                    emit hotkeyHoldStarted(featureId);
                },
                Qt::QueuedConnection);
            swallow = true;
        } else {
            if (!entry.keyDown) {
                continue;
            }
            entry.keyDown = false;
            const QString featureId = QString::fromStdString(entry.featureId);
            QMetaObject::invokeMethod(
                this,
                [this, featureId]() { emit hotkeyHoldEnded(featureId); },
                Qt::QueuedConnection);
            swallow = true;
        }
    }
    return swallow;
}

bool HotkeyManager::handleMouseButtonEvent(int vkCode, bool buttonDown) {
    bool swallow = false;
    for (MouseBindingEntry& entry : m_mouseBindings) {
        if (!entry.binding.matchesVirtualKey(vkCode)) {
            continue;
        }
        if (!entry.binding.modifiersMatch()) {
            if (buttonDown) {
                continue;
            }
        }

        if (entry.holdMode) {
            if (buttonDown) {
                if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
                    continue;
                }
                if (entry.buttonDown) {
                    continue;
                }
                entry.buttonDown = true;
                const QString featureId = QString::fromStdString(entry.featureId);
                const HWND previousForeground = GetForegroundWindow();
                QMetaObject::invokeMethod(
                    this,
                    [this, featureId, previousForeground]() {
                        restoreForegroundWindow(previousForeground);
                        emit hotkeyHoldStarted(featureId);
                    },
                    Qt::QueuedConnection);
                swallow = true;
            } else {
                if (!entry.buttonDown) {
                    continue;
                }
                entry.buttonDown = false;
                const QString featureId = QString::fromStdString(entry.featureId);
                QMetaObject::invokeMethod(
                    this,
                    [this, featureId]() { emit hotkeyHoldEnded(featureId); },
                    Qt::QueuedConnection);
                swallow = true;
            }
            continue;
        }

        if (buttonDown) {
            if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
                continue;
            }
            if (entry.buttonDown) {
                continue;
            }
            entry.buttonDown = true;
            const QString featureId = QString::fromStdString(entry.featureId);
            const HWND previousForeground = GetForegroundWindow();
            QMetaObject::invokeMethod(
                this,
                [this, featureId, previousForeground]() {
                    restoreForegroundWindow(previousForeground);
                    emit hotkeyTriggered(featureId);
                },
                Qt::QueuedConnection);
            swallow = true;
        } else if (entry.buttonDown) {
            entry.buttonDown = false;
            swallow = true;
        }
    }
    return swallow;
}
#endif
