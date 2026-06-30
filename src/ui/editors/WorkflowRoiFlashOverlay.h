#pragma once

#include "core/capture/ScreenCapture.h"

#include <vector>

/// Faint padded ROI border on the target window while an ImageFind block runs.
/// Display rects are expanded outward so the border sits outside the capture haystack.
class WorkflowRoiFlashOverlay {
public:
    static void showSearchArea(SearchArea searchArea,
                               const CaptureRegion& customRegion,
                               const PercentRegion& percentRegion,
                               const std::vector<CaptureRegion>& customRegions);
    static void dismissAll();
};
