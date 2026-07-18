#pragma once

/// Keeps the physical cursor clipped to a fixed screen point while a feature session runs.
class MouseCenterLock {
public:
    static void engageTargetWindowCenter();
    static void engageAtTargetWindowOffset(int offsetX, int offsetY);
    static void engageAt(int screenX, int screenY);
    static bool engageAtCurrentPosition();
    /// Updates the clip point when an early-loop lock is already active (FixedScreenPoint anchor only).
    static void updateFixedLockPoint(int screenX, int screenY);
    static void release();
    static void releaseAll();

    static bool isActive();
    static bool isAnchoredToTargetWindow();
    static void refreshAnchoredPosition();

    /// Suppresses the low-level mouse hook while PIPBONG moves/clicks via SetCursorPos/SendInput.
    static void beginSyntheticPointerOperation();
    static void endSyntheticPointerOperation();
};
