#pragma once

#include <QRect>
#include <QWidget>

#include <functional>
#include <vector>

/// Modeless screen-region picker over the POE target window (Win32 overlay on Windows).
///
/// Architecture is documented in AGENTS.md ("Screen Region Overlay") and
/// .cursor/rules/screen-capture-overlay.mdc — do not replace with a Qt QWidget overlay.
class ScreenRegionOverlay {
public:
    using PickCompletion = std::function<void(bool accepted, const QRect& screenRect)>;

    struct StartPickOptions {
        /// When true (default), moves \p hostWidget's top-level window off-screen during pick so
        /// BitBlt capture in the callback sees the game, not the editor. ROI-only picks can set
        /// false to keep the block editor visible.
        bool parkHostDuringPick = true;
        /// Default true for legacy callers. Template/ROI picks from `ImageFindEditor` set false
        /// so the block editor does not teleport off-screen (selection BitBlts target window only).
        /// Existing ROIs in physical screen pixels; drawn faintly during pick when valid (>= 2 px).
        std::vector<QRect> existingRoiPhysicalRects;
        /// @deprecated Use `existingRoiPhysicalRects`. Single ROI merged into the vector on pick start.
        QRect existingRoiPhysical;
    };

    /// Optionally parks \p hostWidget off-screen during pick (keeps QDialog::exec alive), restores after completion.
    static void startPick(QWidget* hostWidget,
                            PickCompletion onComplete,
                            StartPickOptions options = {});

    /// Destroys any active overlay and restores a parked host without invoking callbacks.
    static void dismissAll();

    /// True while a screen-region pick overlay is active.
    static bool isPickActive();

    /// Runs \p action on the next event-loop tick after the parked host is restored.
    /// Use for modal UI (e.g. capture confirm) so dialogs are not parented to off-screen geometry.
    static void deferUntilHostRestored(std::function<void()> action);
};
