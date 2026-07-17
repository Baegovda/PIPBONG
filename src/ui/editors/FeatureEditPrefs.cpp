#include "ui/editors/FeatureEditPrefs.h"

#include "core/input/HotkeyBinding.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "model/Feature.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/FeatureRunMode.h"
#include "model/UserInputInterruptMode.h"

#include <QSettings>

#include <nlohmann/json.hpp>

namespace {

constexpr const char* kLastFeatureEditSettingsKey = "ui/state/featureEdit/lastSettings_v1";

void applySettingsJson(const nlohmann::json& json, Feature& feature) {
    if (json.contains("enabled")) {
        feature.setEnabled(json.value("enabled", true));
    }
    if (json.contains("runMode")) {
        feature.setRunMode(featureRunModeFromString(json.value("runMode", "RepeatCount")));
    }
    if (json.contains("repeatCount")) {
        feature.setRepeatCount(json.value("repeatCount", 1));
    }
    if (json.contains("infiniteExitAfterConsecutiveMisses")) {
        feature.setInfiniteExitAfterConsecutiveMisses(
            json.value("infiniteExitAfterConsecutiveMisses", 0));
    }
    if (json.contains("userInputInterrupt")) {
        feature.setUserInputInterruptMode(
            userInputInterruptModeFromString(json.value("userInputInterrupt", "Stop")));
    }
    if (json.contains("pointerVisualFeedback")) {
        feature.setPointerVisualFeedback(json.value("pointerVisualFeedback", true));
    }
    if (json.contains("restoreMousePositionOnEnd")) {
        feature.setRestoreMousePositionOnEnd(json.value("restoreMousePositionOnEnd", false));
    }
    if (json.contains("lockMouseDuringFirstLoopCount")) {
        feature.setLockMouseDuringFirstLoopCount(json.value("lockMouseDuringFirstLoopCount", 0));
    }
    if (json.contains("unlockMouseOnBlockFailureCount")) {
        feature.setUnlockMouseOnBlockFailureCount(json.value("unlockMouseOnBlockFailureCount", 1));
    }
    if (json.contains("roiCorrection")) {
        feature.setRoiCorrection(json.value("roiCorrection", false));
    }
    if (json.contains("roiCorrectionExpandPercent")) {
        feature.setRoiCorrectionExpandPercent(
            json.value("roiCorrectionExpandPercent", kDefaultRoiCorrectionExpandPercent));
    }
    if (json.contains("editFirstTemplateRoiOnStart")) {
        feature.setEditFirstTemplateRoiOnStart(json.value("editFirstTemplateRoiOnStart", false));
    }
    if (json.contains("triggerCooldownMs")) {
        feature.setTriggerCooldownMs(
            snapTriggerCooldownMs(json.value("triggerCooldownMs", kDefaultTriggerCooldownMs)));
    }
    if (json.contains("loopIntervalRandomRange")) {
        feature.setLoopIntervalRandomRange(json.value("loopIntervalRandomRange", false));
    }
    if (json.contains("loopIntervalMs")) {
        feature.setLoopIntervalMs(json.value("loopIntervalMs", 0));
    }
    if (json.contains("loopIntervalMinMs")) {
        feature.setLoopIntervalMinMs(json.value("loopIntervalMinMs", 0));
    }
    if (json.contains("loopIntervalMaxMs")) {
        feature.setLoopIntervalMaxMs(json.value("loopIntervalMaxMs", 0));
    }
    if (json.contains("hotkeyAllowExtraModifiers")) {
        feature.setHotkeyAllowExtraModifiers(json.value("hotkeyAllowExtraModifiers", false));
    }
    if (json.contains("captureTargetScope")) {
        feature.setCaptureTargetScope(
            featureCaptureTargetScopeFromString(json.value("captureTargetScope", "Auto")));
    }
    if (json.contains("requireScopedTargetForeground")) {
        feature.setRequireScopedTargetForeground(json.value("requireScopedTargetForeground", false));
    }
    if (json.contains("hotkey")) {
        feature.setHotkey(HotkeyBinding::fromJson(json["hotkey"]));
    } else if (json.contains("hotkeyCleared") && json.value("hotkeyCleared", false)) {
        feature.setHotkey({});
    }
}

nlohmann::json featureSettingsToJson(const Feature& feature) {
    nlohmann::json json;
    json["enabled"] = feature.enabled();
    json["runMode"] = featureRunModeToString(feature.runMode());
    json["repeatCount"] = feature.repeatCount();
    json["infiniteExitAfterConsecutiveMisses"] = feature.infiniteExitAfterConsecutiveMisses();
    json["userInputInterrupt"] = userInputInterruptModeToString(feature.userInputInterruptMode());
    json["pointerVisualFeedback"] = feature.pointerVisualFeedback();
    json["restoreMousePositionOnEnd"] = feature.restoreMousePositionOnEnd();
    json["lockMouseDuringFirstLoopCount"] = feature.lockMouseDuringFirstLoopCount();
    json["unlockMouseOnBlockFailureCount"] = feature.unlockMouseOnBlockFailureCount();
    json["roiCorrection"] = feature.roiCorrection();
    json["roiCorrectionExpandPercent"] = feature.roiCorrectionExpandPercent();
    json["editFirstTemplateRoiOnStart"] = feature.editFirstTemplateRoiOnStart();
    json["triggerCooldownMs"] = feature.triggerCooldownMs();
    json["loopIntervalRandomRange"] = feature.loopIntervalRandomRange();
    json["loopIntervalMs"] = feature.loopIntervalMs();
    json["loopIntervalMinMs"] = feature.loopIntervalMinMs();
    json["loopIntervalMaxMs"] = feature.loopIntervalMaxMs();
    json["hotkeyAllowExtraModifiers"] = feature.hotkeyAllowExtraModifiers();
    json["captureTargetScope"] = featureCaptureTargetScopeToString(feature.captureTargetScope());
    if (feature.requireScopedTargetForeground()) {
        json["requireScopedTargetForeground"] = true;
    }
    if (feature.hotkey().isEmpty()) {
        json["hotkeyCleared"] = true;
    } else {
        json["hotkey"] = feature.hotkey().toJson();
    }
    return json;
}

} // namespace

void applyLastFeatureEditSettings(Feature& feature) {
    QSettings settings;
    const QString raw = settings.value(kLastFeatureEditSettingsKey).toString();
    if (raw.isEmpty()) {
        return;
    }

    try {
        applySettingsJson(nlohmann::json::parse(raw.toStdString()), feature);
    } catch (...) {
    }
}

void saveLastFeatureEditSettings(const Feature& feature) {
    QSettings settings;
    settings.setValue(kLastFeatureEditSettingsKey,
                      QString::fromStdString(featureSettingsToJson(feature).dump()));
}
