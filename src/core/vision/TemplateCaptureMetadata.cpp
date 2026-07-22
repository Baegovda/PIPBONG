#include "core/vision/TemplateCaptureMetadata.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace TemplateCaptureMetadata {
namespace {

constexpr double kResolutionScaleTolerance = 0.005;
constexpr double kResolutionBandHalfWidth = 0.03;
constexpr double kMinMatchScale = 0.1;
constexpr double kMaxMatchScale = 4.0;

} // namespace

std::string metadataPathForTemplate(const std::string& templateAbsolutePath) {
    return templateAbsolutePath + ".capture.json";
}

bool save(const std::string& templateAbsolutePath, int clientWidth, int clientHeight) {
    if (templateAbsolutePath.empty() || clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    const nlohmann::json json{{"targetClientWidth", clientWidth},
                              {"targetClientHeight", clientHeight}};
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

    const int width = json.value("targetClientWidth", 0);
    const int height = json.value("targetClientHeight", 0);
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return ClientSize{width, height};
}

double resolutionScaleFactor(int captureClientWidth,
                             int captureClientHeight,
                             int currentClientWidth,
                             int currentClientHeight) {
    if (captureClientWidth <= 0 || captureClientHeight <= 0 || currentClientWidth <= 0
        || currentClientHeight <= 0) {
        return 1.0;
    }

    const double scaleW = static_cast<double>(currentClientWidth) / captureClientWidth;
    const double scaleH = static_cast<double>(currentClientHeight) / captureClientHeight;
    return std::sqrt(scaleW * scaleH);
}

MatchOptions applyResolutionScale(const MatchOptions& base, double resolutionScale) {
    if (resolutionScale < kMinMatchScale || resolutionScale > kMaxMatchScale) {
        return base;
    }
    if (std::abs(resolutionScale - 1.0) < kResolutionScaleTolerance) {
        return base;
    }

    MatchOptions adjusted = base;
    adjusted.referenceScale = resolutionScale;
    if (base.multiScale) {
        adjusted.minScale = base.minScale * resolutionScale;
        adjusted.maxScale = base.maxScale * resolutionScale;
    } else {
        adjusted.multiScale = true;
        adjusted.minScale = resolutionScale * (1.0 - kResolutionBandHalfWidth);
        adjusted.maxScale = resolutionScale * (1.0 + kResolutionBandHalfWidth);
    }

    adjusted.minScale = std::max(kMinMatchScale, adjusted.minScale);
    adjusted.maxScale = std::min(kMaxMatchScale, std::max(adjusted.minScale + 0.01, adjusted.maxScale));
    return adjusted;
}

MatchOptions matchOptionsForTemplate(const MatchOptions& base,
                                     const std::string& resolvedTemplatePath,
                                     int currentClientWidth,
                                     int currentClientHeight) {
    const std::optional<ClientSize> captureSize = load(resolvedTemplatePath);
    if (!captureSize || currentClientWidth <= 0 || currentClientHeight <= 0) {
        return base;
    }

    const double factor = resolutionScaleFactor(captureSize->width,
                                                captureSize->height,
                                                currentClientWidth,
                                                currentClientHeight);
    return applyResolutionScale(base, factor);
}

} // namespace TemplateCaptureMetadata
