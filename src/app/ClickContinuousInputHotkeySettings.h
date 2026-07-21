#pragma once

#include "core/input/HotkeyBinding.h"

/// Program-wide Click block editor continuous-input arm hotkey (QSettings). Default: F2.
class ClickContinuousInputHotkeySettings {
public:
    static HotkeyBinding binding();
    static void setBinding(const HotkeyBinding& binding);
    static bool isDefaultBinding(const HotkeyBinding& binding);
};
