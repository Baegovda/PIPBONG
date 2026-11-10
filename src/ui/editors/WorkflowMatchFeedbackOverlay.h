#pragma once

#include "app/PointerFeedbackSettings.h"
#include "ui/editors/PointerFeedbackLayers.h"
#include "ui/editors/FeedbackCaptureBarrier.h"
#include <optional>

enum class RunPointerFeedbackKind {
    MatchSuccess,
    MatchMiss,
    TriggerScan,
    Click
};

/// Semi-transparent point pulse on the target window during workflow runs.
class WorkflowMatchFeedbackOverlay {
public:
    static std::optional<PointerFeedbackFrame> frameForWindow(quintptr window);
    static void pulseAtFrame(PointerFeedbackOwner owner,const PointerFeedbackFrame& frame,int x,int y,
                             RunPointerFeedbackKind kind,const std::optional<ClickPointerFeedbackSettings>& feedback={});
    /// \p clientX/\p clientY are coordinates in the target window client area.
    static void pulseAtClientPoint(int clientX, int clientY, RunPointerFeedbackKind kind);
    static void pulseAtClientPoint(int clientX,
                                   int clientY,
                                   RunPointerFeedbackKind kind,
                                   const ClickPointerFeedbackSettings& feedback);
    struct CaptureLease {FeedbackCaptureBarrier::Lease lease;bool ready=false;};
    /// Bounded hide; keep lease through pixel capture. Failed hide must skip capture.
    static CaptureLease prepareCapture();
    static void dismissAll();
};
