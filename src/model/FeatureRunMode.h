#pragma once

#include <nlohmann/json.hpp>

#include <string>

enum class FeatureRunMode {
    Hold,
    RepeatInfinite,
    RepeatCount,
    Trigger
};

std::string featureRunModeToString(FeatureRunMode mode);
FeatureRunMode featureRunModeFromString(const std::string& value);
