#pragma once

#include "core/input/HotkeyBinding.h"

class TemplateCaptureHotkeySettings {
public:
    static HotkeyBinding binding();
    static void setBinding(const HotkeyBinding& binding);
};
