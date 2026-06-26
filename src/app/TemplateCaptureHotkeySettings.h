#pragma once

#include "core/input/HotkeyBinding.h"

/// Program-wide ImageFind template capture hotkey (QSettings, not per-block JSON).
class TemplateCaptureHotkeySettings {
public:
    static HotkeyBinding binding();
    static void setBinding(const HotkeyBinding& binding);
    /// Copies a legacy binding when the global capture hotkey is still unset.
    static void importLegacyIfUnset(const HotkeyBinding& legacy);
};
