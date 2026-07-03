#pragma once

/// Windows current-user Run key registration for boot-time launch.
class WindowsLaunchAtStartup {
public:
    static bool isRegistered();
    static bool setRegistered(bool enabled);
};
