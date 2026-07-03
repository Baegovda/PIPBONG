#pragma once

/// Keeps the physical cursor clipped to a fixed screen point while a feature session runs.
class MouseCenterLock {
public:
    static void engage();
    static void engageAt(int screenX, int screenY);
    static bool engageAtCurrentPosition();
    static void release();
    static void releaseAll();
};
