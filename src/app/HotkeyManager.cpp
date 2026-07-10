#include "app/HotkeyManager.h"

#include "app/FeatureHotkeyGate.h"
#include "core/input/HotkeyBinding.h"
#include "model/Feature.h"
#include "model/FeatureRunMode.h"
#include "model/Project.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QMetaObject>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
namespace {

constexpr wchar_t kHotkeyHostClassName[] = L"PIPBONGHotkeyHost";

HotkeyManager* g_hotkeyManager = nullptr;

LRESULT CALLBACK hotkeyHostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY && g_hotkeyManager) {
        g_hotkeyManager->dispatchWin32Hotkey(static_cast<int>(wParam));
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND createHotkeyHostWindow() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = hotkeyHostWndProc;
        wc.hInstance = instance;
        wc.lpszClassName = kHotkeyHostClassName;
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return nullptr;
        }
        classRegistered = true;
    }

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

LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
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
    if (g_hotkeyManager->handleKeyboardHookEvent(static_cast<int>(info->vkCode), keyDown)) {
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
    uninstallKeyboardHook();
    uninstallMouseHook();
    const HWND host = static_cast<HWND>(m_hotkeyHostHwnd);
    for (const auto& entry : m_idToFeatureId) {
        UnregisterHotKey(host, entry.first);
    }
#endif
    m_idToFeatureId.clear();
    m_holdBindings.clear();
    m_toggleBindings.clear();
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
        if (!feature->enabled()) {
            continue;
        }
        const HotkeyBinding& binding = feature->hotkey();
        if (binding.isEmpty()) {
            continue;
        }

        if (binding.isMouseButton()) {
            MouseBindingEntry entry;
            entry.featureId = feature->id();
            entry.binding = binding;
            entry.allowExtraModifiers = feature->hotkeyAllowExtraModifiers();
            entry.holdMode = feature->runMode() == FeatureRunMode::Hold;
            m_mouseBindings.push_back(entry);
            continue;
        }

        if (feature->runMode() == FeatureRunMode::Hold) {
            HoldBindingEntry entry;
            entry.featureId = feature->id();
            entry.binding = binding;
            entry.allowExtraModifiers = feature->hotkeyAllowExtraModifiers();
            m_holdBindings.push_back(entry);
            continue;
        }

        ToggleBindingEntry entry;
        entry.featureId = feature->id();
        entry.binding = binding;
        entry.allowExtraModifiers = feature->hotkeyAllowExtraModifiers();
        m_toggleBindings.push_back(entry);
    }

    if (!m_holdBindings.empty() || !m_toggleBindings.empty()) {
        installKeyboardHook();
    }
    if (!m_mouseBindings.empty()) {
        installMouseHook();
    }

    if (!m_keyboardHookInstalled) {
        for (const ToggleBindingEntry& entry : m_toggleBindings) {
            if (!registerHotKeyFallback(entry.featureId, entry.binding)) {
                const Feature* feature = project.featureById(entry.featureId);
                failures.push_back({entry.featureId,
                                    feature ? feature->name() : entry.featureId,
                                    GetLastError()});
            }
        }
        for (const HoldBindingEntry& entry : m_holdBindings) {
            const Feature* feature = project.featureById(entry.featureId);
            failures.push_back(
                {entry.featureId, feature ? feature->name() : entry.featureId, ERROR_ACCESS_DENIED});
        }
    }

    if (!m_mouseHookInstalled && !m_mouseBindings.empty()) {
        for (const MouseBindingEntry& entry : m_mouseBindings) {
            const Feature* feature = project.featureById(entry.featureId);
            failures.push_back(
                {entry.featureId, feature ? feature->name() : entry.featureId, ERROR_ACCESS_DENIED});
        }
    }
#else
    (void)project;
#endif

    return failures;
}

#ifdef _WIN32
bool HotkeyManager::registerHotKeyFallback(const std::string& featureId, const HotkeyBinding& binding) {
    const HWND host = static_cast<HWND>(m_hotkeyHostHwnd);
    if (!host || binding.isEmpty()) {
        return false;
    }

    const int id = m_nextId++;
    if (RegisterHotKey(host, id, binding.winModifiers(), static_cast<UINT>(binding.virtualKey))) {
        m_idToFeatureId[id] = featureId;
        return true;
    }
    return false;
}
#endif

bool HotkeyManager::isHoldBindingDown(const std::string& featureId) const {
    // Trust the low-level hook latch only. Do NOT AND with GetAsyncKeyState:
    // workflow KeyPress Tap of the same VK (e.g. Hold Q + "Q 탭") sends a synthetic
    // KEYUP that clears async state while the finger is still down; requiring physical
    // state here aborted the hold every loop and restarted as "루프 1" with no gap.
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

bool HotkeyManager::reconcileHoldBindingDown(const std::string& featureId) {
    for (HoldBindingEntry& entry : m_holdBindings) {
        if (entry.featureId != featureId) {
            continue;
        }
        if (!entry.keyDown) {
            return false;
        }
        if (entry.binding.isPhysicallyDown(entry.allowExtraModifiers)) {
            return true;
        }
        entry.keyDown = false;
        emitHotkeyHoldEnded(entry.featureId);
        return false;
    }
    for (MouseBindingEntry& entry : m_mouseBindings) {
        if (!entry.holdMode || entry.featureId != featureId) {
            continue;
        }
        if (!entry.buttonDown) {
            return false;
        }
        if (entry.binding.isPhysicallyDown(entry.allowExtraModifiers)) {
            return true;
        }
        entry.buttonDown = false;
        emitHotkeyHoldEnded(entry.featureId);
        return false;
    }
    return false;
}

bool HotkeyManager::matchesAnyRegisteredFeatureHotkey(int vkCode) const {
#ifdef _WIN32
    const auto matchesBinding = [vkCode](const HotkeyBinding& binding, bool allowExtraModifiers) {
        if (!binding.matchesVirtualKey(vkCode)) {
            return false;
        }
        if (HotkeyBinding::isMouseVirtualKey(vkCode)) {
            return binding.modifiersMatch(allowExtraModifiers);
        }
        return binding.modifiersMatch(allowExtraModifiers);
    };

    for (const HoldBindingEntry& entry : m_holdBindings) {
        if (matchesBinding(entry.binding, entry.allowExtraModifiers)) {
            return true;
        }
    }
    for (const ToggleBindingEntry& entry : m_toggleBindings) {
        if (matchesBinding(entry.binding, entry.allowExtraModifiers)) {
            return true;
        }
    }
    for (const MouseBindingEntry& entry : m_mouseBindings) {
        if (matchesBinding(entry.binding, entry.allowExtraModifiers)) {
            return true;
        }
    }
#else
    (void)vkCode;
#endif
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

void HotkeyManager::emitHotkeyTriggered(const std::string& featureId) {
    const HWND previousForeground = GetForegroundWindow();
    const QString qFeatureId = QString::fromStdString(featureId);
    QMetaObject::invokeMethod(
        this,
        [this, qFeatureId, previousForeground]() {
            restoreForegroundWindow(previousForeground);
            emit hotkeyTriggered(qFeatureId);
        },
        Qt::QueuedConnection);
}

void HotkeyManager::emitHotkeyHoldStarted(const std::string& featureId) {
    const HWND previousForeground = GetForegroundWindow();
    const QString qFeatureId = QString::fromStdString(featureId);
    QMetaObject::invokeMethod(
        this,
        [this, qFeatureId, previousForeground]() {
            restoreForegroundWindow(previousForeground);
            emit hotkeyHoldStarted(qFeatureId);
        },
        Qt::QueuedConnection);
}

void HotkeyManager::emitHotkeyHoldEnded(const std::string& featureId) {
    const QString qFeatureId = QString::fromStdString(featureId);
    QMetaObject::invokeMethod(
        this,
        [this, qFeatureId]() { emit hotkeyHoldEnded(qFeatureId); },
        Qt::QueuedConnection);
}

void HotkeyManager::dispatchWin32Hotkey(int hotkeyId) {
    handleHotkey(hotkeyId);
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
    emitHotkeyTriggered(it->second);
#else
    emit hotkeyTriggered(QString::fromStdString(it->second));
#endif
}

#ifdef _WIN32
void HotkeyManager::installKeyboardHook() {
    if (m_keyboardHookInstalled) {
        return;
    }
    for (int attempt = 0; attempt < 2 && !m_keyboardHookInstalled; ++attempt) {
        m_keyboardHook =
            SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandleW(nullptr), 0);
        m_keyboardHookInstalled = m_keyboardHook != nullptr;
    }
}

void HotkeyManager::uninstallKeyboardHook() {
    if (m_keyboardHook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_keyboardHook));
        m_keyboardHook = nullptr;
    }
    m_keyboardHookInstalled = false;
    for (HoldBindingEntry& entry : m_holdBindings) {
        entry.keyDown = false;
    }
    for (ToggleBindingEntry& entry : m_toggleBindings) {
        entry.armed = true;
    }
}

void HotkeyManager::installMouseHook() {
    if (m_mouseHookInstalled) {
        return;
    }
    for (int attempt = 0; attempt < 2 && !m_mouseHookInstalled; ++attempt) {
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, GetModuleHandleW(nullptr), 0);
        m_mouseHookInstalled = m_mouseHook != nullptr;
    }
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

bool HotkeyManager::handleKeyboardHookEvent(int vkCode, bool keyDown) {
    if (HotkeyBinding::isMouseVirtualKey(vkCode)) {
        return false;
    }

    const bool hotkeysBlocked = FeatureHotkeyGate::isFeatureHotkeysBlocked();
    bool swallow = false;

    for (HoldBindingEntry& entry : m_holdBindings) {
        if (!entry.binding.matchesVirtualKey(vkCode)) {
            continue;
        }
        if (keyDown) {
            if (!entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
                continue;
            }
            if (hotkeysBlocked) {
                continue;
            }
            swallow = true;
            if (!entry.keyDown) {
                entry.keyDown = true;
                emitHotkeyHoldStarted(entry.featureId);
            }
        } else {
            if (!entry.keyDown) {
                continue;
            }
            if (hotkeysBlocked) {
                continue;
            }
            swallow = true;
            entry.keyDown = false;
            emitHotkeyHoldEnded(entry.featureId);
        }
    }

    for (ToggleBindingEntry& entry : m_toggleBindings) {
        if (!entry.binding.matchesVirtualKey(vkCode)) {
            continue;
        }
        if (keyDown) {
            if (!entry.armed) {
                if (!hotkeysBlocked && entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
                    swallow = true;
                }
                continue;
            }
            if (!entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
                continue;
            }
            if (hotkeysBlocked) {
                continue;
            }
            entry.armed = false;
            emitHotkeyTriggered(entry.featureId);
            swallow = true;
        } else {
            if (!entry.armed && !hotkeysBlocked) {
                swallow = true;
            }
            entry.armed = true;
        }
    }

    return swallow;
}

bool HotkeyManager::handleMouseButtonEvent(int vkCode, bool buttonDown) {
    const bool hotkeysBlocked = FeatureHotkeyGate::isFeatureHotkeysBlocked();
    bool swallow = false;
    for (MouseBindingEntry& entry : m_mouseBindings) {
        if (!entry.binding.matchesVirtualKey(vkCode)) {
            continue;
        }
        if (!entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
            if (buttonDown) {
                continue;
            }
        }

        if (entry.holdMode) {
            if (buttonDown) {
                if (!entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
                    continue;
                }
                if (hotkeysBlocked) {
                    continue;
                }
                swallow = true;
                if (entry.buttonDown) {
                    continue;
                }
                entry.buttonDown = true;
                emitHotkeyHoldStarted(entry.featureId);
            } else {
                if (!entry.buttonDown) {
                    continue;
                }
                if (hotkeysBlocked) {
                    continue;
                }
                swallow = true;
                entry.buttonDown = false;
                emitHotkeyHoldEnded(entry.featureId);
            }
            continue;
        }

        if (buttonDown) {
            if (!entry.buttonDown) {
                if (!entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
                    continue;
                }
                if (hotkeysBlocked) {
                    continue;
                }
                entry.buttonDown = true;
                emitHotkeyTriggered(entry.featureId);
                swallow = true;
            } else if (!hotkeysBlocked && entry.binding.modifiersMatch(entry.allowExtraModifiers)) {
                swallow = true;
            }
        } else if (entry.buttonDown) {
            if (!hotkeysBlocked) {
                swallow = true;
            }
            entry.buttonDown = false;
        }
    }
    return swallow;
}
#endif
