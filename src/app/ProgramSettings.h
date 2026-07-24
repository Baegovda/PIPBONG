#pragma once

#include <QString>
#include <QStringList>

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
        bool pinSubTargetWindowToScreenCenter = false;
        ImageFindCaptureMode imageFindCaptureMode = ImageFindCaptureMode::Hybrid;
        bool runWithoutTargetWindow = false;
        QString linkedTargetProcessPath;
        /// Secondary detection window (e.g. game launcher) — title substring for auto-switch / run target.
        QString subTargetWindowTitle;
        QString subLinkedTargetProcessPath;
        /// Feature ids with trigger mode left armed (monitoring) across restart/profile switch.
        QStringList triggerArmedFeatureIds;
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

    /// When enabled, the profile sub-target window is kept centered on its monitor.
    static bool pinSubTargetWindowToScreenCenter();
    static void setPinSubTargetWindowToScreenCenter(bool enabled);

    /// ImageFind TargetWindow capture strategy.
    static ImageFindCaptureMode imageFindCaptureMode();
    static void setImageFindCaptureMode(ImageFindCaptureMode mode);

    /// When enabled, feature runs are allowed without a designated target window.
    static bool runWithoutTargetWindow();
    static void setRunWithoutTargetWindow(bool enabled);

    /// Maximum retained lines in the execution log panel.
    static int logMaxLines();
    static void setLogMaxLines(int lines);

    /// When enabled, records GUI stalls and CPU spikes to app-spike/latest.md.
    static bool appSpikeProfiling();
    static void setAppSpikeProfiling(bool enabled);

    /// When enabled, switching the active profile activates the linked target window.
    static bool focusTargetWindowOnProfileSelect();
    static void setFocusTargetWindowOnProfileSelect(bool enabled);

    /// Snapshot/apply the profile-scoped subset of settings.
    static ProfileSettings profileSettings();
    static void applyProfileSettings(const ProfileSettings& settings);

    static constexpr int kDefaultUpdateCheckIntervalMinutes = 5;
    static constexpr int kMinUpdateCheckIntervalMinutes = 0;
    static constexpr int kMaxUpdateCheckIntervalMinutes = 1440;

    static constexpr int kDefaultLogMaxLines = 1000;
    static constexpr int kMinLogMaxLines = 100;
    static constexpr int kMaxLogMaxLines = 10000;

    /// Reconcile the Windows Run key with the saved startup preference.
    static void syncWindowsStartupRegistration();
    static void syncWindowsRunAsAdminRegistration();
};
