#pragma once

#include "core/vision/ImageMatcher.h"

#include <optional>
#include <string>
#include <utility>

namespace TemplateCaptureMetadata {

struct ClientSize {
    int width = 0;
    int height = 0;
};

std::string metadataPathForTemplate(const std::string& templateAbsolutePath);

bool save(const std::string& templateAbsolutePath, int clientWidth, int clientHeight);

std::optional<ClientSize> load(const std::string& templateAbsolutePath);

double resolutionScaleFactor(int captureClientWidth,
                             int captureClientHeight,
                             int currentClientWidth,
                             int currentClientHeight);

MatchOptions matchOptionsForTemplate(const MatchOptions& base,
                                     const std::string& resolvedTemplatePath,
                                     int currentClientWidth,
                                     int currentClientHeight);

} // namespace TemplateCaptureMetadata
