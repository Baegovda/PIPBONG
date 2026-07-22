#include "core/vision/TemplateCaptureMetadata.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace TemplateCaptureMetadata {
namespace {

constexpr double kResolutionScaleTolerance = 0.005;
constexpr double kResolutionAutoBandLow = 0.96;
constexpr double kResolutionAutoBandHigh = 1.04;
constexpr double kMinMatchScale = 0.1;
constexpr double kMaxMatchScale = 4.0;

} // namespace

std::string metadataPathForTemplate(const std::string& templateAbsolutePath) {
    return templateAbsolutePath + ".capture.json";
}

bool save(const std::string& templateAbsolutePath, int frameWidth, int frameHeight) {
    if (templateAbsolutePath.empty() || frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    const nlohmann::json json{{"targetFrameWidth", frameWidth},
                              {"targetFrameHeight", frameHeight},
                              {"targetClientWidth", frameWidth},
                              {"targetClientHeight", frameHeight}};
    std::ofstream out(metadataPathForTemplate(templateAbsolutePath));
    if (!out.is_open()) {
        return false;
    }
    out << json.dump(2);
    return out.good();
}

std::optional<ClientSize> load(const std::string& templateAbsolutePath) {
    if (templateAbsolutePath.empty()) {
        return std::nullopt;
    }

    std::ifstream in(metadataPathForTemplate(templateAbsolutePath));
    if (!in.is_open()) {
        return std::nullopt;
    }

    nlohmann::json json;
    try {
        in >> json;
    } catch (...) {
        return std::nullopt;
    }

    int width = json.value("targetFrameWidth", 0);
    int height = json.value("targetFrameHeight", 0);
    if (width <= 0 || height <= 0) {
        width = json.value("targetClientWidth", 0);
        height = json.value("targetClientHeight", 0);
    }
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return ClientSize{width, height};
}

double resolutionScaleFactor(int captureFrameWidth,
                             int captureFrameHeight,
                             int currentFrameWidth,
                             int currentFrameHeight) {
    if (captureFrameWidth <= 0 || captureFrameHeight <= 0 || currentFrameWidth <= 0
        || currentFrameHeight <= 0) {
        return 1.0;
    }

    const double scaleW = static_cast<double>(currentFrameWidth) / captureFrameWidth;
    const double scaleH = static_cast<double>(currentFrameHeight) / captureFrameHeight;
    return std::sqrt(scaleW * scaleH);
}

MatchOptions matchOptionsForTemplate(const MatchOptions& base,
                                     const std::string& resolvedTemplatePath,
                                     int currentFrameWidth,
                                     int currentFrameHeight) {
    const std::optional<ClientSize> captureSize = load(resolvedTemplatePath);
    if (!captureSize || currentFrameWidth <= 0 || currentFrameHeight <= 0) {
        return base;
    }

    const double scaleX =
        static_cast<double>(currentFrameWidth) / static_cast<double>(captureSize->width);
    const double scaleY =
        static_cast<double>(currentFrameHeight) / static_cast<double>(captureSize->height);
    if (scaleX < kMinMatchScale || scaleY < kMinMatchScale || scaleX > kMaxMatchScale
        || scaleY > kMaxMatchScale) {
        return base;
    }
    if (std::abs(scaleX - 1.0) < kResolutionScaleTolerance
        && std::abs(scaleY - 1.0) < kResolutionScaleTolerance) {
        return base;
    }

    const double centerScale = resolutionScaleFactor(captureSize->width,
                                                     captureSize->height,
                                                     currentFrameWidth,
                                                     currentFrameHeight);

    MatchOptions adjusted = base;
    adjusted.referenceScale = centerScale;
    adjusted.resolutionCompensate = true;
    adjusted.multiScale = true;
    if (base.multiScale) {
        adjusted.minScale = std::max(kMinMatchScale, base.minScale * centerScale);
        adjusted.maxScale = std::min(kMaxMatchScale,
                                   std::max(adjusted.minScale + 0.01, base.maxScale * centerScale));
    } else {
        adjusted.minScale = std::max(kMinMatchScale, centerScale * kResolutionAutoBandLow);
        adjusted.maxScale = std::min(kMaxMatchScale,
                                     std::max(adjusted.minScale + 0.01,
                                              centerScale * kResolutionAutoBandHigh));
    }
    return adjusted;
}

} // namespace TemplateCaptureMetadata
