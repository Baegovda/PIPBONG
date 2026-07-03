#pragma once

/// Program-wide preferences persisted in QSettings (not project JSON).
class ProgramSettings {
public:
    static bool autoSelectRunningFeature();
    static void setAutoSelectRunningFeature(bool enabled);

    static bool launchAtWindowsStartup();
    static void setLaunchAtWindowsStartup(bool enabled);

    static bool closeToTray();
    static void setCloseToTray(bool enabled);

    static bool runAsAdministrator();
    static void setRunAsAdministrator(bool enabled);

    static bool autoInstallUpdates();
    static void setAutoInstallUpdates(bool enabled);

    /// Background update-check interval in minutes. 0 disables periodic checks.
    static int updateCheckIntervalMinutes();
    static void setUpdateCheckIntervalMinutes(int minutes);

    static constexpr int kDefaultUpdateCheckIntervalMinutes = 5;
    static constexpr int kMinUpdateCheckIntervalMinutes = 0;
    static constexpr int kMaxUpdateCheckIntervalMinutes = 1440;

    /// Reconcile the Windows Run key with the saved startup preference.
    static void syncWindowsStartupRegistration();
    static void syncWindowsRunAsAdminRegistration();
};
