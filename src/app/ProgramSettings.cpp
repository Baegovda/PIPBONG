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
