#pragma once

#include <QColor>
#include <cstdint>

/// Distinguishes main vs sub target-window bindings for pick/highlight feedback.
enum class TargetWindowBindingRole {
    Main,
    Sub,
};

struct TargetWindowBindingAccentRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

inline TargetWindowBindingAccentRgb targetWindowBindingAccentRgb(TargetWindowBindingRole role) {
    switch (role) {
    case TargetWindowBindingRole::Sub:
        return {30, 136, 229};
    default:
        return {47, 158, 98};
    }
}

inline QColor targetWindowBindingAccentColor(TargetWindowBindingRole role, bool darkTheme) {
    switch (role) {
    case TargetWindowBindingRole::Sub:
        return darkTheme ? QColor(0x64, 0xb5, 0xf6) : QColor(0x1e, 0x88, 0xe5);
    default:
        return darkTheme ? QColor(0x81, 0xc7, 0x84) : QColor(0x2f, 0x9e, 0x62);
    }
}
