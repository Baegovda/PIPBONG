#pragma once
#include <memory>
class MouseLockClaims;

/// Keeps the physical cursor clipped to a fixed screen point while a feature session runs.
class MouseCenterLock {
public:
    static std::shared_ptr<MouseLockClaims> claims();
    static void releaseAll();

    static bool isActive();

    /// Suppresses the low-level mouse hook while PIPBONG moves/clicks via SetCursorPos/SendInput.
    static void beginSyntheticPointerOperation();
    static void endSyntheticPointerOperation();
    static bool syntheticPointerOperationActive();
};
