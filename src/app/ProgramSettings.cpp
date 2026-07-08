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
constexpr const char* kImageFindCaptureModeKey = "program/imageFindCaptureMode";
constexpr const char* kRunWithoutTargetWindowKey = "program/runWithoutTargetWindow";

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

ProgramSettings::ProfileSettings ProgramSettings::profileSettings() {
    ProfileSettings snapshot;
    snapshot.autoSelectRunningFeature = autoSelectRunningFeature();
    snapshot.pinTargetWindowToScreenCenter = pinTargetWindowToScreenCenter();
    snapshot.imageFindCaptureMode = imageFindCaptureMode();
    snapshot.runWithoutTargetWindow = runWithoutTargetWindow();
    return snapshot;
}

void ProgramSettings::applyProfileSettings(const ProfileSettings& settings) {
    setAutoSelectRunningFeature(settings.autoSelectRunningFeature);
    setPinTargetWindowToScreenCenter(settings.pinTargetWindowToScreenCenter);
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
