#include "app/ProgramSettings.h"

#include <QSettings>

namespace {

constexpr const char* kAutoSelectRunningFeatureKey = "program/autoSelectRunningFeature";
constexpr const char* kShowWorkflowRunFeedbackKey = "program/showWorkflowRunFeedback";

} // namespace

bool ProgramSettings::autoSelectRunningFeature() {
    QSettings settings;
    return settings.value(kAutoSelectRunningFeatureKey, true).toBool();
}

void ProgramSettings::setAutoSelectRunningFeature(bool enabled) {
    QSettings settings;
    settings.setValue(kAutoSelectRunningFeatureKey, enabled);
}

bool ProgramSettings::showWorkflowRunFeedback() {
    QSettings settings;
    return settings.value(kShowWorkflowRunFeedbackKey, true).toBool();
}

void ProgramSettings::setShowWorkflowRunFeedback(bool enabled) {
    QSettings settings;
    settings.setValue(kShowWorkflowRunFeedbackKey, enabled);
}
