#pragma once

/// Semi-transparent point pulse on the target window during workflow ImageFind attempts.
class WorkflowMatchFeedbackOverlay {
public:
    /// \p clientX/\p clientY are coordinates in the target window client area.
    static void pulseAtClientPoint(int clientX, int clientY, bool matched);
    /// Hides and clears pulses synchronously before screen capture (safe from worker thread).
    static void hideBeforeCapture();
    static void dismissAll();
};
