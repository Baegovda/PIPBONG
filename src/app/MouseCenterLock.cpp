#include "app/MouseCenterLock.h"

#include "core/capture/ScreenCapture.h"
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
enum class LockAnchor {
    None,
    FixedScreenPoint,
    TargetWindowCenter,
    TargetWindowOffset,
};

int g_refCount = 0;
HHOOK g_mouseHook = nullptr;
POINT g_lockPoint{};
LockAnchor g_anchor = LockAnchor::None;
int g_windowOffsetX = 0;
int g_windowOffsetY = 0;
bool g_appliedOurClip = false;
bool g_savedPreLockClip = false;
RECT g_preLockClipRect{};
int g_syntheticPointerDepth = 0;

// ClipCursor is process-wide. Games (e.g. MOBAs) clip the cursor to the client area;
// calling ClipCursor(nullptr) from PIPBONG clears that clip even when we never engaged
// our own lock. Save the active clip on first engage and restore it on last release.
void savePreLockClipRectIfNeeded() {
    if (g_savedPreLockClip) {
        return;
    }
    RECT rect{};
    if (GetClipCursor(&rect)) {
        g_preLockClipRect = rect;
        g_savedPreLockClip = true;
    }
}

void restorePreLockClipRect() {
    if (!g_appliedOurClip) {
        return;
    }
    if (g_savedPreLockClip) {
        ClipCursor(&g_preLockClipRect);
    } else {
        ClipCursor(nullptr);
    }
    g_appliedOurClip = false;
    g_savedPreLockClip = false;
}

void applyPointClip(const POINT& point, bool moveCursor) {
    if (!g_appliedOurClip) {
        savePreLockClipRectIfNeeded();
    }
    g_lockPoint = point;
    RECT clipRect{g_lockPoint.x, g_lockPoint.y, g_lockPoint.x + 1, g_lockPoint.y + 1};
    ClipCursor(&clipRect);
    g_appliedOurClip = true;
    if (moveCursor) {
        SetCursorPos(g_lockPoint.x, g_lockPoint.y);
    }
}

void installMouseHook();
void uninstallMouseHook();

void refreshAnchoredLockPoint(bool moveCursor) {
    if (g_refCount <= 0) {
        return;
    }

    switch (g_anchor) {
    case LockAnchor::TargetWindowCenter: {
        const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
        if (!target.valid) {
            return;
        }
        POINT point{target.x + target.width / 2, target.y + target.height / 2};
        applyPointClip(point, moveCursor);
        return;
    }
    case LockAnchor::TargetWindowOffset: {
        const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
        if (!target.valid) {
            return;
        }
        POINT point{target.x + g_windowOffsetX, target.y + g_windowOffsetY};
        applyPointClip(point, moveCursor);
        return;
    }
    case LockAnchor::FixedScreenPoint:
        applyPointClip(g_lockPoint, moveCursor);
        return;
    case LockAnchor::None:
    default:
        return;
    }
}

void beginLock(LockAnchor anchor) {
    g_anchor = anchor;
    if (g_refCount++ == 0) {
        refreshAnchoredLockPoint(true);
        installMouseHook();
        return;
    }
    refreshAnchoredLockPoint(true);
}

LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || g_refCount <= 0 || g_syntheticPointerDepth > 0) {
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }

    if (wParam == WM_MOUSEMOVE) {
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (info && !(info->flags & LLMHF_INJECTED)) {
            refreshAnchoredLockPoint(g_anchor == LockAnchor::FixedScreenPoint);
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

void MouseCenterLock::engageTargetWindowCenter() {
#ifdef _WIN32
    beginLock(LockAnchor::TargetWindowCenter);
#else
    (void)0;
#endif
}

void MouseCenterLock::engageAtTargetWindowOffset(int offsetX, int offsetY) {
#ifdef _WIN32
    g_windowOffsetX = offsetX;
    g_windowOffsetY = offsetY;
    beginLock(LockAnchor::TargetWindowOffset);
#else
    (void)offsetX;
    (void)offsetY;
#endif
}

void MouseCenterLock::engageAt(int screenX, int screenY) {
#ifdef _WIN32
    g_lockPoint.x = screenX;
    g_lockPoint.y = screenY;
    beginLock(LockAnchor::FixedScreenPoint);
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

void MouseCenterLock::updateFixedLockPoint(int screenX, int screenY) {
#ifdef _WIN32
    if (g_refCount <= 0 || g_anchor != LockAnchor::FixedScreenPoint) {
        return;
    }
    POINT point{screenX, screenY};
    applyPointClip(point, true);
#else
    (void)screenX;
    (void)screenY;
#endif
}

void MouseCenterLock::release() {
#ifdef _WIN32
    if (g_refCount <= 0) {
        g_refCount = 0;
        return;
    }
    if (--g_refCount == 0) {
        g_anchor = LockAnchor::None;
        uninstallMouseHook();
        restorePreLockClipRect();
    }
#endif
}

void MouseCenterLock::releaseAll() {
#ifdef _WIN32
    if (g_refCount <= 0 && !g_appliedOurClip) {
        g_refCount = 0;
        g_anchor = LockAnchor::None;
        return;
    }
    g_refCount = 0;
    g_anchor = LockAnchor::None;
    uninstallMouseHook();
    restorePreLockClipRect();
#endif
}

bool MouseCenterLock::isActive() {
#ifdef _WIN32
    return g_refCount > 0;
#else
    return false;
#endif
}

bool MouseCenterLock::isAnchoredToTargetWindow() {
#ifdef _WIN32
    return g_refCount > 0
           && (g_anchor == LockAnchor::TargetWindowCenter || g_anchor == LockAnchor::TargetWindowOffset);
#else
    return false;
#endif
}

void MouseCenterLock::refreshAnchoredPosition() {
#ifdef _WIN32
    refreshAnchoredLockPoint(false);
#else
    (void)0;
#endif
}

void MouseCenterLock::beginSyntheticPointerOperation() {
#ifdef _WIN32
    ++g_syntheticPointerDepth;
#else
    (void)0;
#endif
}

void MouseCenterLock::endSyntheticPointerOperation() {
#ifdef _WIN32
    if (g_syntheticPointerDepth > 0) {
        --g_syntheticPointerDepth;
    }
#else
    (void)0;
#endif
}
