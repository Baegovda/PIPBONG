#pragma once

#include <string>

enum class FeatureCaptureTargetScope {
    Auto,
    MainOnly,
    SubOnly
};

std::string featureCaptureTargetScopeToString(FeatureCaptureTargetScope scope);
FeatureCaptureTargetScope featureCaptureTargetScopeFromString(const std::string& value);
