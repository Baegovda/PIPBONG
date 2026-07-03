#include "model/FeatureRunMode.h"

std::string featureRunModeToString(FeatureRunMode mode) {
    switch (mode) {
    case FeatureRunMode::Hold:
        return "Hold";
    case FeatureRunMode::RepeatInfinite:
        return "RepeatInfinite";
    case FeatureRunMode::RepeatCount:
        return "RepeatCount";
    case FeatureRunMode::Trigger:
        return "Trigger";
    }
    return "RepeatCount";
}

FeatureRunMode featureRunModeFromString(const std::string& value) {
    if (value == "Hold") {
        return FeatureRunMode::Hold;
    }
    if (value == "RepeatInfinite") {
        return FeatureRunMode::RepeatInfinite;
    }
    if (value == "Trigger") {
        return FeatureRunMode::Trigger;
    }
    return FeatureRunMode::RepeatCount;
}
