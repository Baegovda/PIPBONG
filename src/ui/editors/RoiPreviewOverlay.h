#pragma once

#include "core/capture/ScreenCapture.h"

#include <QRect>
#include <QWidget>

/// Toggleable Win32 semi-transparent overlay on the target window showing the search ROI.
#include <functional>
#include <string>
#include <vector>

class RoiPreviewOverlay {
public:
    using VisibilityHandler = std::function<void(bool visible)>;
    using RoiIndexHandler = std::function<void(int roiIndex)>;
    using RoiActionHandler = std::function<void()>;
    using RoiRegionHandler = std::function<void(int roiIndex, const CaptureRegion& region)>;

    static bool isVisible();
    static bool show(SearchArea searchArea,
                     const CaptureRegion& customRegion,
                     const PercentRegion& percentRegion,
                     const std::vector<CaptureRegion>& customRegions,
                     QWidget* hostWidget = nullptr,
                     VisibilityHandler onVisibilityChanged = {},
                     int selectedRoiIndex = 0,
                     bool interactive = false,
                     RoiIndexHandler onRoiSelected = {},
                     RoiActionHandler onRoiAdd = {},
                     RoiRegionHandler onRoiRegionChanged = {},
                     RoiIndexHandler onRoiDelete = {});
    static void setSelectedRoiIndex(int selectedRoiIndex);
    static bool hide();
    static bool toggle(SearchArea searchArea,
                       const CaptureRegion& customRegion,
                       const PercentRegion& percentRegion,
                       const std::vector<CaptureRegion>& customRegions,
                       QWidget* hostWidget = nullptr,
                       VisibilityHandler onVisibilityChanged = {});
    static void dismissAll();
};
