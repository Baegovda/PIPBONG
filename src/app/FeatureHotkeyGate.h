#pragma once

// Suppresses feature global hotkeys while:
// - an explicit FeatureHotkeyGateScope is held (block editor, settings, capture), or
// - any QDialog belonging to this process is visible (modal or modeless edit/tool windows).
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
