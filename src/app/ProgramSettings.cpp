#include "app/ProgramSettings.h"

#include "app/WindowsLaunchAtStartup.h"
#include "app/WindowsRunAsAdmin.h"

#include <QSettings>

namespace {

constexpr const char* kAutoSelectRunningFeatureKey = "program/autoSelectRunningFeature";
constexpr const char* kLaunchAtWindowsStartupKey = "program/launchAtWindowsStartup";
constexpr const char* kCloseToTrayKey = "program/closeToTray";
constexpr const char* kRunAsAdministratorKey = "program/runAsAdministrator";
constexpr const char* kAutoInstallUpdatesKey = "program/autoInstallUpdates";
constexpr const char* kUpdateCheckIntervalMinutesKey = "program/updateCheckIntervalMinutes";
constexpr const char* kPinTargetWindowToScreenCenterKey = "program/pinTargetWindowToScreenCenter";
constexpr const char* kPinSubTargetWindowToScreenCenterKey = "program/pinSubTargetWindowToScreenCenter";
constexpr const char* kImageFindCaptureModeKey = "program/imageFindCaptureMode";
constexpr const char* kRunWithoutTargetWindowKey = "program/runWithoutTargetWindow";
constexpr const char* kLogMaxLinesKey = "program/logMaxLines";
constexpr const char* kAppSpikeProfilingKey = "program/appSpikeProfiling";
constexpr const char* kProfileSwitchProfilingKey = "program/profileSwitchProfiling";
constexpr const char* kLegacyCursorStutterProfilingKey = "program/cursorStutterProfiling";
constexpr const char* kFocusTargetWindowOnProfileSelectKey =
    "program/focusTargetWindowOnProfileSelect";

} // namespace

bool ProgramSettings::autoSelectRunningFeature() {
    QSettings settings;
    return settings.value(kAutoSelectRunningFeatureKey, true).toBool();
}

void ProgramSettings::setAutoSelectRunningFeature(bool enabled) {
    QSettings settings;
    settings.setValue(kAutoSelectRunningFeatureKey, enabled);
}

bool ProgramSettings::launchAtWindowsStartup() {
    QSettings settings;
    return settings.value(kLaunchAtWindowsStartupKey, false).toBool();
}

void ProgramSettings::setLaunchAtWindowsStartup(bool enabled) {
    QSettings settings;
    settings.setValue(kLaunchAtWindowsStartupKey, enabled);
    syncWindowsStartupRegistration();
}

bool ProgramSettings::closeToTray() {
    QSettings settings;
    return settings.value(kCloseToTrayKey, false).toBool();
}

void ProgramSettings::setCloseToTray(bool enabled) {
    QSettings settings;
    settings.setValue(kCloseToTrayKey, enabled);
}

bool ProgramSettings::runAsAdministrator() {
    QSettings settings;
    return settings.value(kRunAsAdministratorKey, false).toBool();
}

void ProgramSettings::setRunAsAdministrator(bool enabled) {
    QSettings settings;
    settings.setValue(kRunAsAdministratorKey, enabled);
    syncWindowsRunAsAdminRegistration();
}

bool ProgramSettings::autoInstallUpdates() {
    QSettings settings;
    return settings.value(kAutoInstallUpdatesKey, false).toBool();
}

void ProgramSettings::setAutoInstallUpdates(bool enabled) {
    QSettings settings;
    settings.setValue(kAutoInstallUpdatesKey, enabled);
}

int ProgramSettings::updateCheckIntervalMinutes() {
    QSettings settings;
    int minutes = settings.value(kUpdateCheckIntervalMinutesKey, kDefaultUpdateCheckIntervalMinutes).toInt();
    if (minutes < kMinUpdateCheckIntervalMinutes) {
        minutes = kMinUpdateCheckIntervalMinutes;
    } else if (minutes > kMaxUpdateCheckIntervalMinutes) {
        minutes = kMaxUpdateCheckIntervalMinutes;
    }
    return minutes;
}

void ProgramSettings::setUpdateCheckIntervalMinutes(int minutes) {
    if (minutes < kMinUpdateCheckIntervalMinutes) {
        minutes = kMinUpdateCheckIntervalMinutes;
    } else if (minutes > kMaxUpdateCheckIntervalMinutes) {
        minutes = kMaxUpdateCheckIntervalMinutes;
    }
    QSettings settings;
    settings.setValue(kUpdateCheckIntervalMinutesKey, minutes);
}

bool ProgramSettings::pinTargetWindowToScreenCenter() {
    QSettings settings;
    return settings.value(kPinTargetWindowToScreenCenterKey, false).toBool();
}

void ProgramSettings::setPinTargetWindowToScreenCenter(bool enabled) {
    QSettings settings;
    settings.setValue(kPinTargetWindowToScreenCenterKey, enabled);
}

bool ProgramSettings::pinSubTargetWindowToScreenCenter() {
    QSettings settings;
    return settings.value(kPinSubTargetWindowToScreenCenterKey, false).toBool();
}

void ProgramSettings::setPinSubTargetWindowToScreenCenter(bool enabled) {
    QSettings settings;
    settings.setValue(kPinSubTargetWindowToScreenCenterKey, enabled);
}

ProgramSettings::ImageFindCaptureMode ProgramSettings::imageFindCaptureMode() {
    QSettings settings;
    const int stored = settings.value(
        kImageFindCaptureModeKey,
        static_cast<int>(ImageFindCaptureMode::Hybrid)).toInt();
    if (stored == static_cast<int>(ImageFindCaptureMode::ClientOnly)) {
        return ImageFindCaptureMode::ClientOnly;
    }
    return ImageFindCaptureMode::Hybrid;
}

void ProgramSettings::setImageFindCaptureMode(ImageFindCaptureMode mode) {
    QSettings settings;
    settings.setValue(kImageFindCaptureModeKey, static_cast<int>(mode));
}

bool ProgramSettings::runWithoutTargetWindow() {
    QSettings settings;
    return settings.value(kRunWithoutTargetWindowKey, false).toBool();
}

void ProgramSettings::setRunWithoutTargetWindow(bool enabled) {
    QSettings settings;
    settings.setValue(kRunWithoutTargetWindowKey, enabled);
}

int ProgramSettings::logMaxLines() {
    QSettings settings;
    int lines = settings.value(kLogMaxLinesKey, kDefaultLogMaxLines).toInt();
    if (lines < kMinLogMaxLines) {
        lines = kMinLogMaxLines;
    } else if (lines > kMaxLogMaxLines) {
        lines = kMaxLogMaxLines;
    }
    return lines;
}

void ProgramSettings::setLogMaxLines(int lines) {
    if (lines < kMinLogMaxLines) {
        lines = kMinLogMaxLines;
    } else if (lines > kMaxLogMaxLines) {
        lines = kMaxLogMaxLines;
    }
    QSettings settings;
    settings.setValue(kLogMaxLinesKey, lines);
}

bool ProgramSettings::appSpikeProfiling() {
    QSettings settings;
    if (settings.contains(kAppSpikeProfilingKey)) {
        return settings.value(kAppSpikeProfilingKey, false).toBool();
    }
    return settings.value(kLegacyCursorStutterProfilingKey, false).toBool();
}

void ProgramSettings::setAppSpikeProfiling(bool enabled) {
    QSettings settings;
    settings.setValue(kAppSpikeProfilingKey, enabled);
}

bool ProgramSettings::profileSwitchProfiling() {
    QSettings settings;
    return settings.value(kProfileSwitchProfilingKey, false).toBool();
}

void ProgramSettings::setProfileSwitchProfiling(bool enabled) {
    QSettings settings;
    settings.setValue(kProfileSwitchProfilingKey, enabled);
}

bool ProgramSettings::focusTargetWindowOnProfileSelect() {
    QSettings settings;
    return settings.value(kFocusTargetWindowOnProfileSelectKey, true).toBool();
}

void ProgramSettings::setFocusTargetWindowOnProfileSelect(bool enabled) {
    QSettings settings;
    settings.setValue(kFocusTargetWindowOnProfileSelectKey, enabled);
}

ProgramSettings::ProfileSettings ProgramSettings::profileSettings() {
    ProfileSettings snapshot;
    snapshot.autoSelectRunningFeature = autoSelectRunningFeature();
    snapshot.pinTargetWindowToScreenCenter = pinTargetWindowToScreenCenter();
    snapshot.pinSubTargetWindowToScreenCenter = pinSubTargetWindowToScreenCenter();
    snapshot.imageFindCaptureMode = imageFindCaptureMode();
    snapshot.runWithoutTargetWindow = runWithoutTargetWindow();
    return snapshot;
}

void ProgramSettings::applyProfileSettings(const ProfileSettings& settings) {
    setAutoSelectRunningFeature(settings.autoSelectRunningFeature);
    setPinTargetWindowToScreenCenter(settings.pinTargetWindowToScreenCenter);
    setPinSubTargetWindowToScreenCenter(settings.pinSubTargetWindowToScreenCenter);
    setImageFindCaptureMode(settings.imageFindCaptureMode);
    setRunWithoutTargetWindow(settings.runWithoutTargetWindow);
}

void ProgramSettings::syncWindowsStartupRegistration() {
#ifdef _WIN32
    const bool shouldRegister = launchAtWindowsStartup();
    if (shouldRegister != WindowsLaunchAtStartup::isRegistered()) {
        WindowsLaunchAtStartup::setRegistered(shouldRegister);
    }
#endif
}

void ProgramSettings::syncWindowsRunAsAdminRegistration() {
#ifdef _WIN32
    const bool shouldRegister = runAsAdministrator();
    if (shouldRegister != WindowsRunAsAdmin::isRegistered()) {
        WindowsRunAsAdmin::setRegistered(shouldRegister);
    }
#endif
}
