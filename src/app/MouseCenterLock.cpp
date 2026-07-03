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

POINT virtualScreenCenter() {
    POINT center{};
    center.x = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2;
    center.y = GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2;
    return center;
}

void applyCenterClip() {
    const POINT center = virtualScreenCenter();
    RECT clipRect{center.x, center.y, center.x + 1, center.y + 1};
    ClipCursor(&clipRect);
    SetCursorPos(center.x, center.y);
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
            const POINT center = virtualScreenCenter();
            SetCursorPos(center.x, center.y);
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
