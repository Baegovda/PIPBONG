#include "core/input/InputSimulator.h"

#include "core/workflow/ExecutionContext.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace {

#ifdef _WIN32
constexpr auto kCursorSettleDelay = std::chrono::milliseconds(16);
constexpr auto kTapHoldDelay = std::chrono::milliseconds(12);
/// Games often miss zero-duration / VK-only taps; hold briefly and send scancodes.
constexpr auto kKeyTapHoldDelay = std::chrono::milliseconds(35);
/// When the key is already down (e.g. Hold hotkey = same KeyPress VK), release first
/// so the game sees a real gap before the next down (GetAsyncKeyState / DirectInput).
constexpr auto kKeyPulseGapDelay = std::chrono::milliseconds(30);
constexpr auto kMultiClickGap = std::chrono::milliseconds(8);
constexpr auto kCursorMultiClickGap = std::chrono::milliseconds(1);
constexpr auto kWheelRepeatGap = std::chrono::milliseconds(10);
constexpr int kCursorPositionTolerancePx = 2;
constexpr int kCursorPositionMaxAttempts = 5;

thread_local ExecutionContext* g_activeExecutionContext = nullptr;

void trackSyntheticKey(int virtualKey, bool down) {
    if (!g_activeExecutionContext) {
        return;
    }
    if (down) {
        g_activeExecutionContext->noteSyntheticKeyDown(virtualKey);
    } else {
        g_activeExecutionContext->noteSyntheticKeyUp(virtualKey);
    }
}

void trackSyntheticMouse(MouseButton button, bool down) {
    if (!g_activeExecutionContext || isWheelScrollButton(button)) {
        return;
    }
    if (down) {
        g_activeExecutionContext->noteSyntheticMouseDown(button);
    } else {
        g_activeExecutionContext->noteSyntheticMouseUp(button);
    }
}
#endif

#ifdef _WIN32
DWORD mouseDownFlag(MouseButton button) {
    switch (button) {
    case MouseButton::Left:
        return MOUSEEVENTF_LEFTDOWN;
    case MouseButton::Right:
        return MOUSEEVENTF_RIGHTDOWN;
    case MouseButton::Middle:
        return MOUSEEVENTF_MIDDLEDOWN;
    case MouseButton::Back:
    case MouseButton::Forward:
    case MouseButton::WheelUp:
    case MouseButton::WheelDown:
        break;
    }
    return MOUSEEVENTF_LEFTDOWN;
}

DWORD mouseUpFlag(MouseButton button) {
    switch (button) {
    case MouseButton::Left:
        return MOUSEEVENTF_LEFTUP;
    case MouseButton::Right:
        return MOUSEEVENTF_RIGHTUP;
    case MouseButton::Middle:
        return MOUSEEVENTF_MIDDLEUP;
    case MouseButton::Back:
    case MouseButton::Forward:
    case MouseButton::WheelUp:
    case MouseButton::WheelDown:
        break;
    }
    return MOUSEEVENTF_LEFTUP;
}

DWORD xButtonData(MouseButton button) {
    return button == MouseButton::Back ? XBUTTON1 : XBUTTON2;
}

void sendInputs(const INPUT* inputs, UINT count) {
    SendInput(count, const_cast<INPUT*>(inputs), sizeof(INPUT));
}

bool isAsyncKeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool isExtendedVirtualKey(int virtualKey) {
    switch (virtualKey) {
    case VK_RMENU:
    case VK_RCONTROL:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_NUMLOCK:
    case VK_CANCEL:
    case VK_SNAPSHOT:
    case VK_DIVIDE:
    case VK_APPS:
        return true;
    default:
        return false;
    }
}

void fillKeyboardInput(INPUT& input, int virtualKey, bool down) {
    input = {};
    input.type = INPUT_KEYBOARD;
    // Prefer hardware scancodes (wVk=0). Many games ignore VK-only SendInput.
    const UINT scanEx = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
    const WORD scan = static_cast<WORD>(scanEx & 0xFF);
    const bool extended = isExtendedVirtualKey(virtualKey) || ((scanEx & 0xFF00) != 0);
    DWORD flags = 0;
    if (!down) {
        flags |= KEYEVENTF_KEYUP;
    }
    if (scan != 0) {
        input.ki.wVk = 0;
        input.ki.wScan = scan;
        flags |= KEYEVENTF_SCANCODE;
        if (extended) {
            flags |= KEYEVENTF_EXTENDEDKEY;
        }
    } else {
        input.ki.wVk = static_cast<WORD>(virtualKey);
        input.ki.wScan = 0;
        if (extended) {
            flags |= KEYEVENTF_EXTENDEDKEY;
        }
    }
    input.ki.dwFlags = flags;
}

void sendKeyboardVk(int virtualKey, bool down, bool track = true) {
    INPUT input{};
    fillKeyboardInput(input, virtualKey, down);
    sendInputs(&input, 1);
    if (track) {
        trackSyntheticKey(virtualKey, down);
    }
}

void ensureKeyReleasedUntracked(int virtualKey) {
    if (!isAsyncKeyDown(virtualKey)) {
        return;
    }
    sendKeyboardVk(virtualKey, false, false);
}

void pulseHeldKeyGapUntracked(int virtualKey) {
    if (!isAsyncKeyDown(virtualKey)) {
        return;
    }
    // Brief UP so the game feels the gap. Re-DOWN only when async state dropped after
    // the pulse — if the finger is still down, physical hold already restored async state
    // and an extra synthetic DOWN would leave a stale ref count so GetAsyncKeyState stays
    // true after release and Hold never ends.
    sendKeyboardVk(virtualKey, false, false);
    std::this_thread::sleep_for(kKeyPulseGapDelay);
    if (!isAsyncKeyDown(virtualKey)) {
        sendKeyboardVk(virtualKey, true, false);
    }
}

void sendKeyboardTap(int virtualKey) {
    // Hold hotkey + same-key Tap: pulse only when PIPBONG left a synthetic DOWN in the game.
    // Swallowed physical hold keys are down in GetAsyncKeyState but not tracked — use a
    // normal tap so the first block fires immediately.
    const bool physicallyDown = isAsyncKeyDown(virtualKey);
    const bool pipbongDown = g_activeExecutionContext
                             && g_activeExecutionContext->hasPipbongSyntheticKeyDown(virtualKey);
    if (physicallyDown && pipbongDown) {
        pulseHeldKeyGapUntracked(virtualKey);
        return;
    }
    sendKeyboardVk(virtualKey, true);
    std::this_thread::sleep_for(kKeyTapHoldDelay);
    sendKeyboardVk(virtualKey, false);
}

void setCursorScreenPos(int screenX, int screenY) {
    SetCursorPos(screenX, screenY);
}

bool isCursorNearScreenPos(int screenX, int screenY) {
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return false;
    }
    const int dx = pt.x - screenX;
    const int dy = pt.y - screenY;
    return dx >= -kCursorPositionTolerancePx && dx <= kCursorPositionTolerancePx
           && dy >= -kCursorPositionTolerancePx && dy <= kCursorPositionTolerancePx;
}

void settleCursorAtScreenPos(int screenX, int screenY) {
    if (isCursorNearScreenPos(screenX, screenY)) {
        return;
    }
    constexpr auto kRetryDelay = std::chrono::milliseconds(4);
    for (int attempt = 0; attempt < kCursorPositionMaxAttempts; ++attempt) {
        SetCursorPos(screenX, screenY);
        POINT pt{};
        if (GetCursorPos(&pt)) {
            const int dx = pt.x - screenX;
            const int dy = pt.y - screenY;
            if (dx >= -kCursorPositionTolerancePx && dx <= kCursorPositionTolerancePx
                && dy >= -kCursorPositionTolerancePx && dy <= kCursorPositionTolerancePx) {
                std::this_thread::sleep_for(kCursorSettleDelay);
                return;
            }
        }
        if (attempt + 1 < kCursorPositionMaxAttempts) {
            std::this_thread::sleep_for(kRetryDelay);
        }
    }
    SetCursorPos(screenX, screenY);
    std::this_thread::sleep_for(kCursorSettleDelay);
}

void moveCursorToScreenIfNeeded(int screenX, int screenY, ClickAction action) {
    if (action == ClickAction::Up) {
        return;
    }
    if (!isCursorNearScreenPos(screenX, screenY)) {
        SetCursorPos(screenX, screenY);
    }
}

bool isStandardClickButton(MouseButton button) {
    return button == MouseButton::Left || button == MouseButton::Right
           || button == MouseButton::Middle;
}

void sendStandardButtonDown(MouseButton button, bool track = true) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = mouseDownFlag(button);
    sendInputs(&input, 1);
    if (track) {
        trackSyntheticMouse(button, true);
    }
}

void sendStandardButtonUp(MouseButton button, bool track = true) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = mouseUpFlag(button);
    sendInputs(&input, 1);
    if (track) {
        trackSyntheticMouse(button, false);
    }
}

void sendStandardButtonTap(MouseButton button) {
    sendStandardButtonDown(button);
    std::this_thread::sleep_for(kTapHoldDelay);
    sendStandardButtonUp(button);
}

void sendStandardButtonTapAtCursor(MouseButton button) {
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = mouseDownFlag(button);
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = mouseUpFlag(button);
    sendInputs(inputs, 2);
    trackSyntheticMouse(button, true);
    trackSyntheticMouse(button, false);
}

void sendXButtonDown(MouseButton button, bool track = true) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_XDOWN;
    input.mi.mouseData = xButtonData(button);
    sendInputs(&input, 1);
    if (track) {
        trackSyntheticMouse(button, true);
    }
}

void sendXButtonUp(MouseButton button, bool track = true) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_XUP;
    input.mi.mouseData = xButtonData(button);
    sendInputs(&input, 1);
    if (track) {
        trackSyntheticMouse(button, false);
    }
}

void sendXButtonTap(MouseButton button) {
    sendXButtonDown(button);
    std::this_thread::sleep_for(kTapHoldDelay);
    sendXButtonUp(button);
}

void sendXButtonTapAtCursor(MouseButton button) {
    const DWORD data = xButtonData(button);
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
    inputs[0].mi.mouseData = data;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_XUP;
    inputs[1].mi.mouseData = data;
    sendInputs(inputs, 2);
    trackSyntheticMouse(button, true);
    trackSyntheticMouse(button, false);
}

bool isMouseButtonPhysicallyDown(MouseButton button) {
    switch (button) {
    case MouseButton::Left:
        return isAsyncKeyDown(VK_LBUTTON);
    case MouseButton::Right:
        return isAsyncKeyDown(VK_RBUTTON);
    case MouseButton::Middle:
        return isAsyncKeyDown(VK_MBUTTON);
    case MouseButton::Back:
        return isAsyncKeyDown(VK_XBUTTON1);
    case MouseButton::Forward:
        return isAsyncKeyDown(VK_XBUTTON2);
    case MouseButton::WheelUp:
    case MouseButton::WheelDown:
        break;
    }
    return false;
}

void clampClientPoint(HWND hwnd, int& clientX, int& clientY) {
    RECT client{};
    if (!GetClientRect(hwnd, &client)) {
        return;
    }
    const int maxX = (client.right > 0) ? (client.right - 1) : 0;
    const int maxY = (client.bottom > 0) ? (client.bottom - 1) : 0;
    if (clientX < 0) {
        clientX = 0;
    } else if (clientX > maxX) {
        clientX = maxX;
    }
    if (clientY < 0) {
        clientY = 0;
    } else if (clientY > maxY) {
        clientY = maxY;
    }
}

bool getCursorClientPoint(HWND hwnd, int& clientX, int& clientY) {
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return false;
    }
    if (!ScreenToClient(hwnd, &pt)) {
        return false;
    }
    clientX = pt.x;
    clientY = pt.y;
    clampClientPoint(hwnd, clientX, clientY);
    return true;
}

WPARAM clientButtonDownWParam(MouseButton button, const KeyModifiers& mods) {
    WPARAM wParam = 0;
    if (mods.shift) {
        wParam |= MK_SHIFT;
    }
    if (mods.ctrl) {
        wParam |= MK_CONTROL;
    }
    if (button != MouseButton::Left && isAsyncKeyDown(VK_LBUTTON)) {
        wParam |= MK_LBUTTON;
    }
    if (button != MouseButton::Right && isAsyncKeyDown(VK_RBUTTON)) {
        wParam |= MK_RBUTTON;
    }
    if (button != MouseButton::Middle && isAsyncKeyDown(VK_MBUTTON)) {
        wParam |= MK_MBUTTON;
    }
    switch (button) {
    case MouseButton::Left:
        wParam |= MK_LBUTTON;
        break;
    case MouseButton::Right:
        wParam |= MK_RBUTTON;
        break;
    case MouseButton::Middle:
        wParam |= MK_MBUTTON;
        break;
    case MouseButton::Back:
    case MouseButton::Forward:
    case MouseButton::WheelUp:
    case MouseButton::WheelDown:
        break;
    }
    return wParam;
}

WPARAM clientButtonUpWParam(MouseButton button, const KeyModifiers& mods) {
    WPARAM wParam = clientButtonDownWParam(button, mods);
    switch (button) {
    case MouseButton::Left:
        wParam &= ~static_cast<WPARAM>(MK_LBUTTON);
        break;
    case MouseButton::Right:
        wParam &= ~static_cast<WPARAM>(MK_RBUTTON);
        break;
    case MouseButton::Middle:
        wParam &= ~static_cast<WPARAM>(MK_MBUTTON);
        break;
    case MouseButton::Back:
    case MouseButton::Forward:
    case MouseButton::WheelUp:
    case MouseButton::WheelDown:
        break;
    }
    return wParam;
}

struct ClientMouseMessages {
    UINT downMsg = 0;
    UINT upMsg = 0;
};

bool clientMouseMessages(MouseButton button, ClientMouseMessages& out) {
    switch (button) {
    case MouseButton::Left:
        out.downMsg = WM_LBUTTONDOWN;
        out.upMsg = WM_LBUTTONUP;
        return true;
    case MouseButton::Right:
        out.downMsg = WM_RBUTTONDOWN;
        out.upMsg = WM_RBUTTONUP;
        return true;
    case MouseButton::Middle:
        out.downMsg = WM_MBUTTONDOWN;
        out.upMsg = WM_MBUTTONUP;
        return true;
    default:
        return false;
    }
}

WPARAM physicalMouseMoveWParam();

void postClientButtonTap(HWND hwnd,
                         int clientX,
                         int clientY,
                         MouseButton button,
                         const KeyModifiers& mods) {
    ClientMouseMessages msgs{};
    if (!clientMouseMessages(button, msgs)) {
        return;
    }

    const LPARAM lParam = MAKELPARAM(static_cast<WORD>(clientX & 0xFFFF),
                                     static_cast<WORD>(clientY & 0xFFFF));
    PostMessage(hwnd, WM_MOUSEMOVE, physicalMouseMoveWParam(), lParam);
    PostMessage(hwnd, msgs.downMsg, clientButtonDownWParam(button, mods), lParam);
    PostMessage(hwnd, msgs.upMsg, clientButtonUpWParam(button, mods), lParam);
}

void performClientCursorTap(HWND hwnd, MouseButton button, int count, const KeyModifiers& mods) {
    for (int i = 0; i < count; ++i) {
        int clientX = 0;
        int clientY = 0;
        if (!getCursorClientPoint(hwnd, clientX, clientY)) {
            return;
        }
        postClientButtonTap(hwnd, clientX, clientY, button, mods);
        if (i + 1 < count) {
            std::this_thread::sleep_for(kCursorMultiClickGap);
        }
    }
}

void performStandardButtonActionAtCursor(MouseButton button, ClickAction action, int count) {
    switch (action) {
    case ClickAction::Down:
        sendStandardButtonDown(button);
        break;
    case ClickAction::Up:
        sendStandardButtonUp(button);
        break;
    case ClickAction::Tap:
    default:
        for (int i = 0; i < count; ++i) {
            sendStandardButtonTapAtCursor(button);
            if (i + 1 < count) {
                std::this_thread::sleep_for(kCursorMultiClickGap);
            }
        }
        break;
    }
}

void performXButtonActionAtCursor(MouseButton button, ClickAction action, int count) {
    switch (action) {
    case ClickAction::Down:
        sendXButtonDown(button);
        break;
    case ClickAction::Up:
        sendXButtonUp(button);
        break;
    case ClickAction::Tap:
    default:
        for (int i = 0; i < count; ++i) {
            sendXButtonTapAtCursor(button);
            if (i + 1 < count) {
                std::this_thread::sleep_for(kCursorMultiClickGap);
            }
        }
        break;
    }
}

void performStandardButtonAction(MouseButton button, ClickAction action, int count) {
    switch (action) {
    case ClickAction::Down:
        sendStandardButtonDown(button);
        break;
    case ClickAction::Up:
        sendStandardButtonUp(button);
        break;
    case ClickAction::Tap:
    default:
        for (int i = 0; i < count; ++i) {
            sendStandardButtonTap(button);
            if (i + 1 < count) {
                std::this_thread::sleep_for(kMultiClickGap);
            }
        }
        break;
    }
}

void clickStandardAtScreen(int screenX,
                           int screenY,
                           MouseButton button,
                           ClickAction action,
                           int count) {
    const bool needsCursorMove = action != ClickAction::Up;
    if (needsCursorMove) {
        settleCursorAtScreenPos(screenX, screenY);
    }
    performStandardButtonAction(button, action, count);
}

void clickXButtonAtScreen(int screenX,
                          int screenY,
                          MouseButton button,
                          ClickAction action,
                          int count) {
    const bool needsCursorMove =
        action != ClickAction::Up;
    if (needsCursorMove) {
        settleCursorAtScreenPos(screenX, screenY);
    }
    switch (action) {
    case ClickAction::Down:
        sendXButtonDown(button);
        break;
    case ClickAction::Up:
        sendXButtonUp(button);
        break;
    case ClickAction::Tap:
    default:
        for (int i = 0; i < count; ++i) {
            sendXButtonTap(button);
            if (i + 1 < count) {
                std::this_thread::sleep_for(kMultiClickGap);
            }
        }
        break;
    }
}

void sendXButton(MouseButton button, ClickAction action, int count) {
    const DWORD data = xButtonData(button);
    switch (action) {
    case ClickAction::MoveOnly:
        break;
    case ClickAction::Down: {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_XDOWN;
        input.mi.mouseData = data;
        sendInputs(&input, 1);
        trackSyntheticMouse(button, true);
        break;
    }
    case ClickAction::Up: {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_XUP;
        input.mi.mouseData = data;
        sendInputs(&input, 1);
        trackSyntheticMouse(button, false);
        break;
    }
    case ClickAction::Tap:
    default:
        for (int i = 0; i < count; ++i) {
            sendXButtonTap(button);
            if (i + 1 < count) {
                std::this_thread::sleep_for(kMultiClickGap);
            }
        }
        break;
    }
}

void sendWheelScroll(MouseButton button, int count) {
    const int delta = button == MouseButton::WheelUp ? WHEEL_DELTA : -WHEEL_DELTA;
    for (int i = 0; i < count; ++i) {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(delta);
        sendInputs(&input, 1);
        if (i + 1 < count) {
            std::this_thread::sleep_for(kWheelRepeatGap);
        }
    }
}

void pressModifier(int vk, bool down) {
    sendKeyboardVk(vk, down);
}

struct AppliedKeyModifiers {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
};

struct ModifierSnapshot {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

bool isShiftPhysicallyDown() {
    return isAsyncKeyDown(VK_SHIFT) || isAsyncKeyDown(VK_LSHIFT) || isAsyncKeyDown(VK_RSHIFT);
}

bool isCtrlPhysicallyDown() {
    return isAsyncKeyDown(VK_CONTROL) || isAsyncKeyDown(VK_LCONTROL) || isAsyncKeyDown(VK_RCONTROL);
}

bool isAltPhysicallyDown() {
    return isAsyncKeyDown(VK_MENU) || isAsyncKeyDown(VK_LMENU) || isAsyncKeyDown(VK_RMENU);
}

ModifierSnapshot captureModifierSnapshot() {
    return {isShiftPhysicallyDown(), isCtrlPhysicallyDown(), isAltPhysicallyDown()};
}

bool isModifierVirtualKey(int virtualKey) {
    switch (virtualKey) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

bool isModifierVirtualKeyPhysicallyDown(int virtualKey) {
    switch (virtualKey) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return isShiftPhysicallyDown();
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return isCtrlPhysicallyDown();
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return isAltPhysicallyDown();
    default:
        return false;
    }
}

AppliedKeyModifiers pressModifiersIfNeeded(const KeyModifiers& mods,
                                           const ModifierSnapshot& beforeBlock) {
    AppliedKeyModifiers applied;
    if (mods.ctrl && !beforeBlock.ctrl) {
        pressModifier(VK_CONTROL, true);
        applied.ctrl = true;
    }
    if (mods.alt && !beforeBlock.alt) {
        pressModifier(VK_MENU, true);
        applied.alt = true;
    }
    if (mods.shift && !beforeBlock.shift) {
        pressModifier(VK_SHIFT, true);
        applied.shift = true;
    }
    return applied;
}

WPARAM physicalMouseMoveWParam() {
    WPARAM wParam = 0;
    if (isShiftPhysicallyDown()) {
        wParam |= MK_SHIFT;
    }
    if (isCtrlPhysicallyDown()) {
        wParam |= MK_CONTROL;
    }
    if (isAsyncKeyDown(VK_LBUTTON)) {
        wParam |= MK_LBUTTON;
    }
    if (isAsyncKeyDown(VK_RBUTTON)) {
        wParam |= MK_RBUTTON;
    }
    if (isAsyncKeyDown(VK_MBUTTON)) {
        wParam |= MK_MBUTTON;
    }
    return wParam;
}

void releaseAppliedModifiers(const AppliedKeyModifiers& applied,
                             const ModifierSnapshot& beforeBlock) {
    // Release only modifiers PIPBONG pressed for this block. Never KEYUP keys the user
    // already held before the block started (beforeBlock snapshot).
    if (applied.shift && !beforeBlock.shift) {
        pressModifier(VK_SHIFT, false);
    }
    if (applied.alt && !beforeBlock.alt) {
        pressModifier(VK_MENU, false);
    }
    if (applied.ctrl && !beforeBlock.ctrl) {
        pressModifier(VK_CONTROL, false);
    }
}
#endif

} // namespace

void InputSimulator::moveMouse(int screenX, int screenY) {
#ifdef _WIN32
    setCursorScreenPos(screenX, screenY);
#endif
}

void InputSimulator::moveAt(int screenX, int screenY) {
    moveMouse(screenX, screenY);
}

void InputSimulator::click(MouseButton button, int count) {
    mouseButton(button, ClickAction::Tap, count);
}

void InputSimulator::mouseButton(MouseButton button, ClickAction action, int count) {
#ifdef _WIN32
    if (isWheelScrollButton(button)) {
        sendWheelScroll(button, count);
        return;
    }
    if (button == MouseButton::Back || button == MouseButton::Forward) {
        sendXButton(button, action, count);
        return;
    }

    switch (action) {
    case ClickAction::MoveOnly:
        break;
    case ClickAction::Down: {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = mouseDownFlag(button);
        sendInputs(&input, 1);
        trackSyntheticMouse(button, true);
        break;
    }
    case ClickAction::Up: {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = mouseUpFlag(button);
        sendInputs(&input, 1);
        trackSyntheticMouse(button, false);
        break;
    }
    case ClickAction::Tap:
    default:
        for (int i = 0; i < count; ++i) {
            sendStandardButtonTap(button);
            if (i + 1 < count) {
                std::this_thread::sleep_for(kMultiClickGap);
            }
        }
        break;
    }
#endif
}

void InputSimulator::clickAt(int screenX,
                             int screenY,
                             MouseButton button,
                             int count,
                             KeyModifiers mods) {
    clickAt(screenX, screenY, button, ClickAction::Tap, count, mods);
}

void InputSimulator::clickAt(int screenX,
                             int screenY,
                             MouseButton button,
                             ClickAction action,
                             int count,
                             KeyModifiers mods) {
#ifdef _WIN32
    if (action == ClickAction::MoveOnly) {
        moveAt(screenX, screenY);
        return;
    }

    const bool needsModifiers = mods.any();
    ModifierSnapshot beforeBlock{};
    AppliedKeyModifiers appliedMods{};
    if (needsModifiers) {
        beforeBlock = captureModifierSnapshot();
        if (action == ClickAction::Tap || action == ClickAction::Down) {
            appliedMods = pressModifiersIfNeeded(mods, beforeBlock);
        }
    }

    if (isWheelScrollButton(button)) {
        settleCursorAtScreenPos(screenX, screenY);
        sendWheelScroll(button, count);
    } else if (button == MouseButton::Back || button == MouseButton::Forward) {
        clickXButtonAtScreen(screenX, screenY, button, action, count);
    } else if (isStandardClickButton(button)) {
        clickStandardAtScreen(screenX, screenY, button, action, count);
    }

    if (needsModifiers && action == ClickAction::Tap) {
        releaseAppliedModifiers(appliedMods, beforeBlock);
    }
#endif
}

void InputSimulator::clickAtCursor(MouseButton button,
                                   ClickAction action,
                                   int count,
                                   KeyModifiers mods) {
#ifdef _WIN32
    if (action == ClickAction::MoveOnly) {
        return;
    }

    const bool needsModifiers = mods.any();
    ModifierSnapshot beforeBlock{};
    AppliedKeyModifiers appliedMods{};
    if (needsModifiers) {
        beforeBlock = captureModifierSnapshot();
        if (action == ClickAction::Tap || action == ClickAction::Down) {
            appliedMods = pressModifiersIfNeeded(mods, beforeBlock);
        }
    }

    if (isWheelScrollButton(button)) {
        sendWheelScroll(button, count);
    } else if (button == MouseButton::Back || button == MouseButton::Forward) {
        performXButtonActionAtCursor(button, action, count);
    } else if (isStandardClickButton(button)) {
        performStandardButtonActionAtCursor(button, action, count);
    }

    if (needsModifiers && action == ClickAction::Tap) {
        releaseAppliedModifiers(appliedMods, beforeBlock);
    }
#endif
}

bool InputSimulator::shouldUseClientCursorClick(HWND hwnd, MouseButton button, ClickAction action) {
#ifdef _WIN32
    if (!hwnd || !IsWindow(hwnd) || action != ClickAction::Tap) {
        return false;
    }
    if (isWheelScrollButton(button)) {
        return false;
    }
    if (!isStandardClickButton(button) && button != MouseButton::Back && button != MouseButton::Forward) {
        return false;
    }
    return isMouseButtonPhysicallyDown(button);
#else
    (void)hwnd;
    (void)button;
    (void)action;
    return false;
#endif
}

void InputSimulator::clickAtCursorOnTarget(HWND hwnd,
                                           MouseButton button,
                                           ClickAction action,
                                           int count,
                                           KeyModifiers mods) {
#ifdef _WIN32
    if (!hwnd || !IsWindow(hwnd) || action == ClickAction::MoveOnly) {
        return;
    }

    const bool needsModifiers = mods.any();
    ModifierSnapshot beforeBlock{};
    AppliedKeyModifiers appliedMods{};
    if (needsModifiers) {
        beforeBlock = captureModifierSnapshot();
        if (action == ClickAction::Tap || action == ClickAction::Down) {
            appliedMods = pressModifiersIfNeeded(mods, beforeBlock);
        }
    }

    if (isWheelScrollButton(button)) {
        sendWheelScroll(button, count);
    } else if (button == MouseButton::Back || button == MouseButton::Forward) {
        performXButtonActionAtCursor(button, action, count);
    } else if (isStandardClickButton(button)) {
        switch (action) {
        case ClickAction::Down: {
            int clientX = 0;
            int clientY = 0;
            if (!getCursorClientPoint(hwnd, clientX, clientY)) {
                break;
            }
            ClientMouseMessages msgs{};
            if (!clientMouseMessages(button, msgs)) {
                break;
            }
            const LPARAM lParam = MAKELPARAM(static_cast<WORD>(clientX & 0xFFFF),
                                             static_cast<WORD>(clientY & 0xFFFF));
            PostMessage(hwnd, WM_MOUSEMOVE, physicalMouseMoveWParam(), lParam);
            PostMessage(hwnd, msgs.downMsg, clientButtonDownWParam(button, mods), lParam);
            trackSyntheticMouse(button, true);
            break;
        }
        case ClickAction::Up: {
            int clientX = 0;
            int clientY = 0;
            if (!getCursorClientPoint(hwnd, clientX, clientY)) {
                break;
            }
            ClientMouseMessages msgs{};
            if (!clientMouseMessages(button, msgs)) {
                break;
            }
            const LPARAM lParam = MAKELPARAM(static_cast<WORD>(clientX & 0xFFFF),
                                             static_cast<WORD>(clientY & 0xFFFF));
            PostMessage(hwnd, msgs.upMsg, clientButtonUpWParam(button, mods), lParam);
            trackSyntheticMouse(button, false);
            break;
        }
        case ClickAction::Tap:
        default:
            performClientCursorTap(hwnd, button, count, mods);
            break;
        }
    }

    if (needsModifiers && action == ClickAction::Tap) {
        releaseAppliedModifiers(appliedMods, beforeBlock);
    }
#else
    (void)hwnd;
    (void)button;
    (void)action;
    (void)count;
    (void)mods;
#endif
}

void InputSimulator::clickAtMatchScreen(int screenX,
                                        int screenY,
                                        MouseButton button,
                                        ClickAction action,
                                        int count,
                                        KeyModifiers mods) {
#ifdef _WIN32
    if (action == ClickAction::MoveOnly) {
        moveCursorToScreenIfNeeded(screenX, screenY, action);
        return;
    }

    const bool needsModifiers = mods.any();
    ModifierSnapshot beforeBlock{};
    AppliedKeyModifiers appliedMods{};
    if (needsModifiers) {
        beforeBlock = captureModifierSnapshot();
        if (action == ClickAction::Tap || action == ClickAction::Down) {
            appliedMods = pressModifiersIfNeeded(mods, beforeBlock);
        }
    }

    moveCursorToScreenIfNeeded(screenX, screenY, action);

    if (isWheelScrollButton(button)) {
        sendWheelScroll(button, count);
    } else if (button == MouseButton::Back || button == MouseButton::Forward) {
        performXButtonActionAtCursor(button, action, count);
    } else if (isStandardClickButton(button)) {
        performStandardButtonActionAtCursor(button, action, count);
    }

    if (needsModifiers && action == ClickAction::Tap) {
        releaseAppliedModifiers(appliedMods, beforeBlock);
    }
#endif
}

#ifdef _WIN32
bool InputSimulator::clientToScreen(HWND hwnd, int clientX, int clientY, int& screenX, int& screenY) {
    POINT pt{clientX, clientY};
    if (!ClientToScreen(hwnd, &pt)) {
        return false;
    }
    screenX = pt.x;
    screenY = pt.y;
    return true;
}

bool InputSimulator::getCursorScreenPosition(int& screenX, int& screenY) {
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return false;
    }
    screenX = pt.x;
    screenY = pt.y;
    return true;
}

bool InputSimulator::screenToClient(HWND hwnd, int screenX, int screenY, int& clientX, int& clientY) {
    POINT pt{screenX, screenY};
    if (!ScreenToClient(hwnd, &pt)) {
        return false;
    }
    clientX = pt.x;
    clientY = pt.y;
    return true;
}

void InputSimulator::clickAtClient(HWND hwnd,
                                   int clientX,
                                   int clientY,
                                   MouseButton button,
                                   int count,
                                   KeyModifiers mods) {
    clickAtClient(hwnd, clientX, clientY, button, ClickAction::Tap, count, mods);
}

void InputSimulator::clickAtClient(HWND hwnd,
                                   int clientX,
                                   int clientY,
                                   MouseButton button,
                                   ClickAction action,
                                   int count,
                                   KeyModifiers mods) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    // Keep clicks inside the client area so the cursor does not jump outside the game.
    RECT client{};
    if (GetClientRect(hwnd, &client)) {
        const int maxX = (client.right > 0) ? (client.right - 1) : 0;
        const int maxY = (client.bottom > 0) ? (client.bottom - 1) : 0;
        if (clientX < 0) {
            clientX = 0;
        } else if (clientX > maxX) {
            clientX = maxX;
        }
        if (clientY < 0) {
            clientY = 0;
        } else if (clientY > maxY) {
            clientY = maxY;
        }
    }

    if (action == ClickAction::MoveOnly) {
        moveAtClient(hwnd, clientX, clientY);
        return;
    }

    int screenX = 0;
    int screenY = 0;
    if (!clientToScreen(hwnd, clientX, clientY, screenX, screenY)) {
        return;
    }

    // SetCursorPos + SendInput button events at screen coords. SendMessage-only clicks
    // let games miss input; MOUSEEVENTF_ABSOLUTE without VIRTUALDESK mis-maps multi-monitor coords.
    clickAt(screenX, screenY, button, action, count, mods);
}

void InputSimulator::moveAtClient(HWND hwnd, int clientX, int clientY) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    const LPARAM lParam = MAKELPARAM(static_cast<WORD>(clientX & 0xFFFF),
                                     static_cast<WORD>(clientY & 0xFFFF));
    SendMessage(hwnd, WM_MOUSEMOVE, physicalMouseMoveWParam(), lParam);
}
#endif

void InputSimulator::ensureKeyReleased(int virtualKey) {
#ifdef _WIN32
    ensureKeyReleasedUntracked(virtualKey);
#else
    (void)virtualKey;
#endif
}

void InputSimulator::forceKeyUp(int virtualKey) {
#ifdef _WIN32
    sendKeyboardVk(virtualKey, false, false);
#else
    (void)virtualKey;
#endif
}

void InputSimulator::forceHotkeyMouseButtonUp(int virtualKey) {
#ifdef _WIN32
    switch (virtualKey) {
    case VK_LBUTTON:
        sendStandardButtonUp(MouseButton::Left, false);
        break;
    case VK_RBUTTON:
        sendStandardButtonUp(MouseButton::Right, false);
        break;
    case VK_MBUTTON:
        sendStandardButtonUp(MouseButton::Middle, false);
        break;
    case VK_XBUTTON1:
        sendXButtonUp(MouseButton::Back, false);
        break;
    case VK_XBUTTON2:
        sendXButtonUp(MouseButton::Forward, false);
        break;
    default:
        break;
    }
#else
    (void)virtualKey;
#endif
}

void InputSimulator::pulseHeldKeyGap(int virtualKey) {
#ifdef _WIN32
    pulseHeldKeyGapUntracked(virtualKey);
#else
    (void)virtualKey;
#endif
}

void InputSimulator::sendKey(int virtualKey,
                             KeyAction action,
                             const KeyPressModifierActions& mods,
                             bool sendMainKey) {
#ifdef _WIN32
    if (sendMainKey && action == KeyAction::Tap && isModifierVirtualKey(virtualKey)
        && isModifierVirtualKeyPhysicallyDown(virtualKey)) {
        return;
    }

    const ModifierSnapshot beforeBlock = captureModifierSnapshot();
    AppliedKeyModifiers applied;

    const auto applyModifierDownOrTap = [&](ModifierKeyAction modAction,
                                          bool beforeHeld,
                                          int vk,
                                          bool AppliedKeyModifiers::* appliedField) {
        if (modAction == ModifierKeyAction::None || modAction == ModifierKeyAction::Up) {
            return;
        }
        if (beforeHeld) {
            return;
        }
        if (modAction == ModifierKeyAction::Down) {
            pressModifier(vk, true);
            applied.*appliedField = true;
            return;
        }
        sendKeyboardTap(vk);
    };

    const auto applyModifierUp = [&](ModifierKeyAction modAction, bool beforeHeld, int vk) {
        if (modAction != ModifierKeyAction::Up || beforeHeld) {
            return;
        }
        if (vk == VK_CONTROL && isCtrlPhysicallyDown()) {
            pressModifier(VK_CONTROL, false);
        } else if (vk == VK_MENU && isAltPhysicallyDown()) {
            pressModifier(VK_MENU, false);
        } else if (vk == VK_SHIFT && isShiftPhysicallyDown()) {
            pressModifier(VK_SHIFT, false);
        }
    };

    applyModifierDownOrTap(mods.ctrl, beforeBlock.ctrl, VK_CONTROL, &AppliedKeyModifiers::ctrl);
    applyModifierDownOrTap(mods.alt, beforeBlock.alt, VK_MENU, &AppliedKeyModifiers::alt);
    applyModifierDownOrTap(mods.shift, beforeBlock.shift, VK_SHIFT, &AppliedKeyModifiers::shift);

    if (sendMainKey) {
        switch (action) {
        case KeyAction::Down:
            sendKeyboardVk(virtualKey, true);
            break;
        case KeyAction::Up:
            sendKeyboardVk(virtualKey, false);
            break;
        case KeyAction::Tap:
            sendKeyboardTap(virtualKey);
            break;
        }
    }

    applyModifierUp(mods.ctrl, beforeBlock.ctrl, VK_CONTROL);
    applyModifierUp(mods.alt, beforeBlock.alt, VK_MENU);
    applyModifierUp(mods.shift, beforeBlock.shift, VK_SHIFT);

    if (sendMainKey && action == KeyAction::Tap) {
        releaseAppliedModifiers(applied, beforeBlock);
    }
#endif
}

void InputSimulator::sendText(const std::wstring& text) {
#ifdef _WIN32
    for (wchar_t ch : text) {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wScan = ch;
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        sendInputs(inputs, 2);
    }
#endif
}

#ifdef _WIN32
SessionModifierSnapshot InputSimulator::captureSessionModifierSnapshot() {
    const ModifierSnapshot snap = captureModifierSnapshot();
    return {snap.shift, snap.ctrl, snap.alt};
}

void InputSimulator::setActiveExecutionContext(ExecutionContext* context) {
    g_activeExecutionContext = context;
}

int normalizeModifierVirtualKey(int virtualKey) {
    switch (virtualKey) {
    case VK_LSHIFT:
    case VK_RSHIFT:
        return VK_SHIFT;
    case VK_LCONTROL:
    case VK_RCONTROL:
        return VK_CONTROL;
    case VK_LMENU:
    case VK_RMENU:
        return VK_MENU;
    default:
        return virtualKey;
    }
}

void InputSimulator::restoreTrackedKeyboard(std::unordered_set<int>& heldKeys,
                                            const SessionModifierSnapshot& /*sessionStart*/) {
    const auto keysToRelease = heldKeys;
    for (int virtualKey : keysToRelease) {
        sendKeyboardVk(normalizeModifierVirtualKey(virtualKey), false, false);
        heldKeys.erase(virtualKey);
    }
}

void InputSimulator::restoreTrackedMouseButtons(std::unordered_set<int>& heldButtons) {
    const auto buttonsToRelease = heldButtons;
    for (int rawButton : buttonsToRelease) {
        const auto button = static_cast<MouseButton>(rawButton);
        if (isWheelScrollButton(button)) {
            heldButtons.erase(rawButton);
            continue;
        }
        if (button == MouseButton::Back || button == MouseButton::Forward) {
            sendXButtonUp(button, false);
        } else {
            sendStandardButtonUp(button, false);
        }
        heldButtons.erase(rawButton);
    }
}
#endif
