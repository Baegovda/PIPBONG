#pragma once

#include "ui/TargetWindowBindingRole.h"

class QWidget;

/// Brief pulsing border highlight on the current target window (Win32 layered overlay).
class TargetWindowHighlightOverlay {
public:
    static bool flash(QWidget* hostWidget = nullptr,
                      TargetWindowBindingRole role = TargetWindowBindingRole::Main);
    static bool flashSelectionWave(QWidget* hostWidget = nullptr,
                                   TargetWindowBindingRole role = TargetWindowBindingRole::Main);
#ifdef _WIN32
    static bool flashForHwnd(void* hwnd,
                             QWidget* hostWidget = nullptr,
                             TargetWindowBindingRole role = TargetWindowBindingRole::Main);
    static bool flashSelectionWaveForHwnd(void* hwnd,
                                          QWidget* hostWidget = nullptr,
                                          TargetWindowBindingRole role = TargetWindowBindingRole::Main);
#endif
    static void dismissAll();
};
