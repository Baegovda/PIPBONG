#pragma once

#include <nlohmann/json.hpp>

#include <string>

enum class FeatureRunMode {
    Toggle, // Legacy JSON alias; treated as RepeatCount (typically 1 run).
    Hold,
    RepeatInfinite,
    RepeatCount
};

FeatureRunMode normalizeRunMode(FeatureRunMode mode);

std::string featureRunModeToString(FeatureRunMode mode);
FeatureRunMode featureRunModeFromString(const std::string& value);
