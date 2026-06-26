#include "app/ProgramSettings.h"

#include <QSettings>

namespace {

constexpr const char* kAutoSelectRunningFeatureKey = "program/autoSelectRunningFeature";

} // namespace

bool ProgramSettings::autoSelectRunningFeature() {
    QSettings settings;
    return settings.value(kAutoSelectRunningFeatureKey, true).toBool();
}

void ProgramSettings::setAutoSelectRunningFeature(bool enabled) {
    QSettings settings;
    settings.setValue(kAutoSelectRunningFeatureKey, enabled);
}
