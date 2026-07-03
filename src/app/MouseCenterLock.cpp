#include "app/MouseCenterLock.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
int g_refCount = 0;
HHOOK g_mouseHook = nullptr;
POINT g_lockPoint{};

POINT virtualScreenCenter() {
    POINT center{};
    center.x = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2;
    center.y = GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2;
    return center;
}

void applyCenterClip() {
    const POINT center = virtualScreenCenter();
    g_lockPoint = center;
    RECT clipRect{g_lockPoint.x, g_lockPoint.y, g_lockPoint.x + 1, g_lockPoint.y + 1};
    ClipCursor(&clipRect);
    SetCursorPos(g_lockPoint.x, g_lockPoint.y);
}

void applyPointClip(const POINT& point) {
    g_lockPoint = point;
    RECT clipRect{g_lockPoint.x, g_lockPoint.y, g_lockPoint.x + 1, g_lockPoint.y + 1};
    ClipCursor(&clipRect);
    SetCursorPos(g_lockPoint.x, g_lockPoint.y);
}

void installMouseHook();
void uninstallMouseHook();

LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || g_refCount <= 0) {
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }

    if (wParam == WM_MOUSEMOVE) {
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (info && !(info->flags & LLMHF_INJECTED)) {
            SetCursorPos(g_lockPoint.x, g_lockPoint.y);
            return 1;
        }
    }

    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

void installMouseHook() {
    if (g_mouseHook) {
        return;
    }
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, GetModuleHandleW(nullptr), 0);
}

void uninstallMouseHook() {
    if (!g_mouseHook) {
        return;
    }
    UnhookWindowsHookEx(g_mouseHook);
    g_mouseHook = nullptr;
}

#endif

} // namespace

void MouseCenterLock::engage() {
#ifdef _WIN32
    if (g_refCount++ == 0) {
        applyCenterClip();
        installMouseHook();
    } else {
        applyCenterClip();
    }
#else
    (void)0;
#endif
}

void MouseCenterLock::engageAt(int screenX, int screenY) {
#ifdef _WIN32
    POINT point{screenX, screenY};
    if (g_refCount++ == 0) {
        applyPointClip(point);
        installMouseHook();
    } else {
        applyPointClip(point);
    }
#else
    (void)screenX;
    (void)screenY;
#endif
}

bool MouseCenterLock::engageAtCurrentPosition() {
#ifdef _WIN32
    POINT point{};
    if (!GetCursorPos(&point)) {
        return false;
    }
    engageAt(point.x, point.y);
    return true;
#else
    return false;
#endif
}

void MouseCenterLock::release() {
#ifdef _WIN32
    if (g_refCount <= 0) {
        g_refCount = 0;
        return;
    }
    if (--g_refCount == 0) {
        uninstallMouseHook();
        ClipCursor(nullptr);
    }
#endif
}

void MouseCenterLock::releaseAll() {
#ifdef _WIN32
    g_refCount = 0;
    uninstallMouseHook();
    ClipCursor(nullptr);
#endif
}
