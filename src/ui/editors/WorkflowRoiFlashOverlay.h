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

/// Subtle persistent ROI outline on the target window while an ImageFind block is selected
/// in the workflow block list (editor feedback only; separate HWND from run flash overlay).
class WorkflowImageFindSelectionRoiOverlay {
public:
    static void showSearchArea(SearchArea searchArea,
                               const CaptureRegion& customRegion,
                               const PercentRegion& percentRegion,
                               const std::vector<CaptureRegion>& customRegions);
    static void dismissAll();
};
