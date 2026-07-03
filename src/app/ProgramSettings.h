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

    /// Reconcile the Windows Run key with the saved startup preference.
    static void syncWindowsStartupRegistration();
    static void syncWindowsRunAsAdminRegistration();
};
