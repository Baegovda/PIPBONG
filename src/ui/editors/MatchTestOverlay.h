#pragma once

#include "core/capture/ScreenCapture.h"
#include "core/vision/ImageMatcher.h"

#include <QWidget>

#include <functional>
#include <vector>

#include <cstdint>

/// Win32 semi-transparent overlay on the target window showing ImageFind match-test hits.
class MatchTestOverlay {
public:
    using VisibilityHandler = std::function<void(bool visible)>;

    static bool isVisible();
    static bool show(const std::vector<MatchResult>& matches,
                     double threshold,
                     SearchArea searchArea,
                     const CaptureRegion& customRegion,
                     const PercentRegion& percentRegion,
                     QWidget* hostWidget = nullptr,
                     VisibilityHandler onVisibilityChanged = {},
                     const std::vector<std::pair<CaptureRegion, std::vector<MatchResult>>>*
                         matchesPerCustomRegion = nullptr,
                     int64_t matchDurationMs = -1);
    static bool hide();
    static void dismissAll();
};
