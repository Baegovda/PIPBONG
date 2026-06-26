#pragma once

#include "core/input/HotkeyBinding.h"

/// Program-wide Click block editor cursor-position hotkey (QSettings). Default: F1.
class ClickCursorHotkeySettings {
public:
    static HotkeyBinding binding();
    static void setBinding(const HotkeyBinding& binding);
    static bool isDefaultBinding(const HotkeyBinding& binding);
};
