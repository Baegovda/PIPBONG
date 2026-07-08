#pragma once

/// Program-wide preferences persisted in QSettings (not project JSON).
class ProgramSettings {
public:
    enum class ImageFindCaptureMode {
        Hybrid = 0,
        ClientOnly = 1,
    };

    struct ProfileSettings {
        bool autoSelectRunningFeature = true;
        bool pinTargetWindowToScreenCenter = false;
        ImageFindCaptureMode imageFindCaptureMode = ImageFindCaptureMode::Hybrid;
        bool runWithoutTargetWindow = false;
    };

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

    /// When enabled, the designated target window is kept centered on its monitor.
    static bool pinTargetWindowToScreenCenter();
    static void setPinTargetWindowToScreenCenter(bool enabled);

    /// ImageFind TargetWindow capture strategy.
    static ImageFindCaptureMode imageFindCaptureMode();
    static void setImageFindCaptureMode(ImageFindCaptureMode mode);

    /// When enabled, feature runs are allowed without a designated target window.
    static bool runWithoutTargetWindow();
    static void setRunWithoutTargetWindow(bool enabled);

    /// Snapshot/apply the profile-scoped subset of settings.
    static ProfileSettings profileSettings();
    static void applyProfileSettings(const ProfileSettings& settings);

    static constexpr int kDefaultUpdateCheckIntervalMinutes = 5;
    static constexpr int kMinUpdateCheckIntervalMinutes = 0;
    static constexpr int kMaxUpdateCheckIntervalMinutes = 1440;

    /// Reconcile the Windows Run key with the saved startup preference.
    static void syncWindowsStartupRegistration();
    static void syncWindowsRunAsAdminRegistration();
};
