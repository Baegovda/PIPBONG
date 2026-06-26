#include "model/FeatureRunMode.h"

FeatureRunMode normalizeRunMode(FeatureRunMode mode) {
    if (mode == FeatureRunMode::Toggle) {
        return FeatureRunMode::RepeatCount;
    }
    return mode;
}

std::string featureRunModeToString(FeatureRunMode mode) {
    switch (normalizeRunMode(mode)) {
    case FeatureRunMode::Hold:
        return "Hold";
    case FeatureRunMode::RepeatInfinite:
        return "RepeatInfinite";
    case FeatureRunMode::RepeatCount:
        return "RepeatCount";
    case FeatureRunMode::Toggle:
    default:
        return "RepeatCount";
    }
}

FeatureRunMode featureRunModeFromString(const std::string& value) {
    if (value == "Hold") {
        return FeatureRunMode::Hold;
    }
    if (value == "RepeatInfinite") {
        return FeatureRunMode::RepeatInfinite;
    }
    if (value == "RepeatCount" || value == "Toggle") {
        return FeatureRunMode::RepeatCount;
    }
    return FeatureRunMode::RepeatCount;
}
