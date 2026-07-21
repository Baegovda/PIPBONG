#pragma once

#include <functional>

// Suppresses feature global hotkeys while:
// - an explicit FeatureHotkeyGateScope is held (block editor, settings, capture), or
// - any QDialog belonging to this process is visible (modal or modeless edit/tool windows), or
// - focus is in a text/numeric field inside a hotkey-gate-exempt tool dialog (memo, calculator, CPU watch).
class FeatureHotkeyGate {
public:
    static bool isFeatureHotkeysBlocked();

    /** When true, low-level keyboard hooks must not swallow the key (e.g. F2 feature-list rename). */
    static void setKeyboardHookDeferPredicate(std::function<bool(int vkCode, bool keyDown)> predicate);
    static bool shouldDeferKeyboardHook(int vkCode, bool keyDown);

private:
    friend class FeatureHotkeyGateScope;
    static void addBlocker();
    static void removeBlocker();
};

class FeatureHotkeyGateScope {
public:
    FeatureHotkeyGateScope();
    ~FeatureHotkeyGateScope();

    FeatureHotkeyGateScope(const FeatureHotkeyGateScope&) = delete;
    FeatureHotkeyGateScope& operator=(const FeatureHotkeyGateScope&) = delete;

private:
    bool m_active = true;
};
