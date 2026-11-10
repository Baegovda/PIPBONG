#pragma once

#include "core/capture/ScreenCapture.h"

#include <QRect>
#include <QPointer>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <memory>

/// Toggleable Win32 semi-transparent overlay on the target window showing the search ROI.
class RoiPreviewOverlay {
    friend class RoiPreviewOwnershipChecks;
public:
    using Owner = std::shared_ptr<void>;
    using VisibilityHandler = std::function<void(bool visible)>;
    using RoiEditedHandler = std::function<void(int index, const CaptureRegion& region)>;
    using RoiSelectedHandler = std::function<void(int index)>;

    struct EditableOptions {
        Owner owner;
        bool enabled = false;
        int activeIndex = 0;
        // First-start edits use one physical coordinate frame for the entire
        // preview instead of following later shared capture-target changes.
        std::optional<QRect> fixedTargetBounds;
        RoiEditedHandler onRoiEdited;
        RoiSelectedHandler onRoiSelected;
        std::function<void()> onConfirm;
        std::function<void()> onAdd;
        std::function<void()> onDelete;
    };

    static bool isVisible(const Owner& owner = {});
    static bool isEditable(const Owner& owner = {});
    static bool show(SearchArea searchArea,
                     const CaptureRegion& customRegion,
                     const PercentRegion& percentRegion,
                     const std::vector<CaptureRegion>& customRegions,
                     QWidget* hostWidget = nullptr,
                     VisibilityHandler onVisibilityChanged = {},
                     const EditableOptions& editable = {});
    static bool hide(const Owner& owner = {});
    static bool toggle(SearchArea searchArea,
                       const CaptureRegion& customRegion,
                       const PercentRegion& percentRegion,
                       const std::vector<CaptureRegion>& customRegions,
                       QWidget* hostWidget = nullptr,
                       VisibilityHandler onVisibilityChanged = {},
                       const EditableOptions& editable = {});
    static void setActiveRoiIndex(int index, const Owner& owner = {});
    static void dismiss(const Owner& owner);
    static void dismissAll();
private:
    struct NativeRequest {
        SearchArea searchArea; CaptureRegion customRegion; PercentRegion percentRegion;
        std::vector<CaptureRegion> customRegions;
        QPointer<QWidget> host;
        VisibilityHandler visibility;
        EditableOptions editable;
        std::function<bool()> current;
    };
    struct Backend {
        std::function<bool(const NativeRequest&)> show;
        std::function<bool()> visible, editable;
        std::function<void()> dismiss;
        std::function<void(int)> select;
    };
    struct Control;
    static std::shared_ptr<Control> control();
    static Backend replaceBackendForChecks(Backend);
    static bool nativeShow(const NativeRequest&);
    static bool nativeVisible();
    static bool nativeEditable();
    static void nativeDismiss();
    static void nativeSelect(int);
};
