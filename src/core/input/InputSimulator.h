#pragma once

#include <string>
#include <unordered_set>

enum class MouseButton {
    Left,
    Right,
    Middle,
    Back,
    Forward,
    WheelUp,
    WheelDown
};

inline bool isWheelScrollButton(MouseButton button) {
    return button == MouseButton::WheelUp || button == MouseButton::WheelDown;
}

enum class KeyAction {
    Tap,
    Down,
    Up
};

enum class ClickAction {
    Tap,
    Down,
    Up,
    MoveOnly
};

struct KeyModifiers {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    bool any() const { return ctrl || alt || shift; }
};

enum class ModifierKeyAction {
    None,
    Tap,
    Down,
    Up
};

struct KeyPressModifierActions {
    ModifierKeyAction ctrl = ModifierKeyAction::None;
    ModifierKeyAction alt = ModifierKeyAction::None;
    ModifierKeyAction shift = ModifierKeyAction::None;

    bool hasAny() const {
        return ctrl != ModifierKeyAction::None || alt != ModifierKeyAction::None
               || shift != ModifierKeyAction::None;
    }
};

#ifdef _WIN32
struct SessionModifierSnapshot {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};
#endif

class ExecutionContext;

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class InputSimulator {
public:
    static void moveMouse(int screenX, int screenY);
    static void moveAt(int screenX, int screenY);
    static void click(MouseButton button, int count = 1);
    static void mouseButton(MouseButton button, ClickAction action, int count = 1);
    static void clickAt(int screenX,
                        int screenY,
                        MouseButton button,
                        int count = 1,
                        KeyModifiers mods = {});
    static void clickAt(int screenX,
                        int screenY,
                        MouseButton button,
                        ClickAction action,
                        int count = 1,
                        KeyModifiers mods = {});
    static void clickAtCursor(MouseButton button,
                              ClickAction action,
                              int count = 1,
                              KeyModifiers mods = {});
    static void clickAtMatchScreen(int screenX,
                                   int screenY,
                                   MouseButton button,
                                   ClickAction action,
                                   int count = 1,
                                   KeyModifiers mods = {});

#ifdef _WIN32
    static void clickAtClient(HWND hwnd,
                              int clientX,
                              int clientY,
                              MouseButton button,
                              int count = 1,
                              KeyModifiers mods = {});
    static void clickAtClient(HWND hwnd,
                              int clientX,
                              int clientY,
                              MouseButton button,
                              ClickAction action,
                              int count = 1,
                              KeyModifiers mods = {});
    static void moveAtClient(HWND hwnd, int clientX, int clientY);
    static bool clientToScreen(HWND hwnd, int clientX, int clientY, int& screenX, int& screenY);
    static bool getCursorScreenPosition(int& screenX, int& screenY);
    static bool screenToClient(HWND hwnd, int screenX, int screenY, int& clientX, int& clientY);
#endif

    static void sendKey(int virtualKey,
                        KeyAction action,
                        const KeyPressModifierActions& mods = {},
                        bool sendMainKey = true);
    /// If the VK is currently down (physical or synthetic), send an untracked KEYUP so
    /// games see a release — used for Hold+same-key Tap gaps and loop-interval spacing.
    static void ensureKeyReleased(int virtualKey);
    /// Brief untracked UP then DOWN so games feel a gap while the finger still holds
    /// (restores down state so a later physical KEYUP can end Hold mode).
    static void pulseHeldKeyGap(int virtualKey);
    static void sendText(const std::wstring& text);

#ifdef _WIN32
    static SessionModifierSnapshot captureSessionModifierSnapshot();
    static void setActiveExecutionContext(ExecutionContext* context);
    static void restoreTrackedKeyboard(std::unordered_set<int>& heldKeys,
                                       const SessionModifierSnapshot& sessionStart);
    static void restoreTrackedMouseButtons(std::unordered_set<int>& heldButtons);
#endif
};
