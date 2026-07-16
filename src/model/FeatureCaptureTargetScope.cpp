#include "model/FeatureCaptureTargetScope.h"

std::string featureCaptureTargetScopeToString(FeatureCaptureTargetScope scope) {
    switch (scope) {
    case FeatureCaptureTargetScope::MainOnly:
        return "MainOnly";
    case FeatureCaptureTargetScope::SubOnly:
        return "SubOnly";
    case FeatureCaptureTargetScope::Auto:
    default:
        return "Auto";
    }
}

FeatureCaptureTargetScope featureCaptureTargetScopeFromString(const std::string& value) {
    if (value == "MainOnly") {
        return FeatureCaptureTargetScope::MainOnly;
    }
    if (value == "SubOnly") {
        return FeatureCaptureTargetScope::SubOnly;
    }
    return FeatureCaptureTargetScope::Auto;
}
