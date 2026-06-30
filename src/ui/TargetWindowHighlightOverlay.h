#pragma once

class QWidget;

/// Brief pulsing border highlight on the current target window (Win32 layered overlay).
class TargetWindowHighlightOverlay {
public:
    static bool flash(QWidget* hostWidget = nullptr);
    static void dismissAll();
};
