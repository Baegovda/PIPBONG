#pragma once

class QWidget;

/// Brief Win32 border pulse on the current target window (main window "현재 창?" feedback).
class TargetWindowHighlightOverlay {
public:
    static bool isActive();
    static bool flash(QWidget* hostWidget = nullptr);
    static void dismissAll();
};
