#pragma once

/// Keeps the physical cursor clipped to a fixed screen point while a feature session runs.
class MouseCenterLock {
public:
    static void engageTargetWindowCenter();
    static void engageAtTargetWindowOffset(int offsetX, int offsetY);
    static void engageAt(int screenX, int screenY);
    static bool engageAtCurrentPosition();
    static void release();
    static void releaseAll();

    static bool isActive();
    static bool isAnchoredToTargetWindow();
    static void refreshAnchoredPosition();
};
