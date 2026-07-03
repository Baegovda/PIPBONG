#pragma once

/// Keeps the physical cursor clipped to the virtual-screen center while a feature session runs.
class MouseCenterLock {
public:
    static void engage();
    static void release();
    static void releaseAll();
};
