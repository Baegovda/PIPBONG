#pragma once

/// Windows compatibility-layer flag so PIPBONG always launches elevated (UAC).
class WindowsRunAsAdmin {
public:
    static bool isRegistered();
    static bool setRegistered(bool enabled);
    static bool isProcessElevated();
    static bool relaunchElevated();
};
