#pragma once

class QWidget;

/// Brief pulsing border highlight on the current target window (Win32 layered overlay).
class TargetWindowHighlightOverlay {
public:
    static bool flash(QWidget* hostWidget = nullptr);
    static bool flashSelectionWave(QWidget* hostWidget = nullptr);
#ifdef _WIN32
    static bool flashForHwnd(void* hwnd, QWidget* hostWidget = nullptr);
    static bool flashSelectionWaveForHwnd(void* hwnd, QWidget* hostWidget = nullptr);
#endif
    static void dismissAll();
};
