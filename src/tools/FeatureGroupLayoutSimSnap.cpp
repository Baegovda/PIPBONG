// Minimal snap helpers for FeatureGroupLayoutSim (same logic as WaitBlock / ImageFindBlock).
#include "core/workflow/blocks/WaitBlock.h"

#include <algorithm>

int snapWaitDelayMs(int ms) {
    ms = std::max(0, ms);
    return ((ms + kWaitDelayStepMs / 2) / kWaitDelayStepMs) * kWaitDelayStepMs;
}

int snapRoiCorrectionExpandPercent(int percent) {
    constexpr int kMin = 50;
    constexpr int kMax = 300;
    constexpr int kStep = 5;
    percent = std::clamp(percent, kMin, kMax);
    return ((percent + kStep / 2) / kStep) * kStep;
}

#include "ui/FeatureRunModeTheme.h"

QColor featureRunModeTriggerWatchAccent(bool /*darkTheme*/) {
    return QColor(0x4e, 0xa8, 0x8c);
}

QColor featureRunModeTriggerCooldownAccent(bool /*darkTheme*/) {
    return QColor(0xd4, 0xa5, 0x74);
}
