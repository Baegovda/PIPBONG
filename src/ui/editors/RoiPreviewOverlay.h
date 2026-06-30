#pragma once

#include "core/capture/ScreenCapture.h"

#include <QRect>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

/// Toggleable Win32 semi-transparent overlay on the target window showing the search ROI.
class RoiPreviewOverlay {
public:
    using VisibilityHandler = std::function<void(bool visible)>;
    using RoiEditedHandler = std::function<void(int index, const CaptureRegion& region)>;
    using RoiSelectedHandler = std::function<void(int index)>;

    struct EditableOptions {
        bool enabled = false;
        int activeIndex = 0;
        RoiEditedHandler onRoiEdited;
        RoiSelectedHandler onRoiSelected;
        std::function<void()> onConfirm;
        std::function<void()> onAdd;
        std::function<void()> onDelete;
    };

    static bool isVisible();
    static bool isEditable();
    static bool show(SearchArea searchArea,
                     const CaptureRegion& customRegion,
                     const PercentRegion& percentRegion,
                     const std::vector<CaptureRegion>& customRegions,
                     QWidget* hostWidget = nullptr,
                     VisibilityHandler onVisibilityChanged = {},
                     const EditableOptions& editable = {});
    static bool hide();
    static bool toggle(SearchArea searchArea,
                       const CaptureRegion& customRegion,
                       const PercentRegion& percentRegion,
                       const std::vector<CaptureRegion>& customRegions,
                       QWidget* hostWidget = nullptr,
                       VisibilityHandler onVisibilityChanged = {},
                       const EditableOptions& editable = {});
    static void setActiveRoiIndex(int index);
    static void dismissAll();
};
