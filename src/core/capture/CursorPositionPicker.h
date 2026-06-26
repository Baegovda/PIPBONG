#pragma once

#include <functional>

class QWidget;

class CursorPositionPicker {
public:
    struct Result {
        bool accepted = false;
        int x = 0;
        int y = 0;
    };

    using Completion = std::function<void(const Result&)>;

    /// Show countdown tooltip at cursor, then record position after delaySeconds.
    /// When useClientCoordinates is true, converts via ScreenCapture target window when available.
    static void startPick(QWidget* hostWidget,
                          bool useClientCoordinates,
                          int delaySeconds,
                          Completion callback);
    static void cancelPick();
    static bool isActive();
};
