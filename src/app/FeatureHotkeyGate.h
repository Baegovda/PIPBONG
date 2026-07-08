#pragma once

// Suppresses feature global hotkeys while modal UI (e.g. block editor) is open
// or while a feature hotkey binding capture session is active.
class FeatureHotkeyGate {
public:
    static bool isFeatureHotkeysBlocked();

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
