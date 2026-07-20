#pragma once

#include "model/FeatureRunMode.h"

#include <QColor>

struct FeatureRunModeTheme {
    QColor accent;
    QColor chipFill;
    QColor chipBorder;
    QColor chipText;
    QColor rowWash;
};

inline FeatureRunModeTheme featureRunModeTheme(FeatureRunMode mode, bool darkTheme) {
    FeatureRunModeTheme theme;
    switch (mode) {
    case FeatureRunMode::Hold:
        if (darkTheme) {
            theme.accent = QColor(0xd4, 0xa5, 0x74);
            theme.chipFill = QColor(0x2a, 0x22, 0x16);
            theme.chipBorder = QColor(0x8a, 0x6a, 0x3a);
            theme.chipText = QColor(0xf0, 0xd8, 0xb0);
            theme.rowWash = QColor(0xd4, 0xa5, 0x74);
        } else {
            theme.accent = QColor(0xa0, 0x68, 0x20);
            theme.chipFill = QColor(0xff, 0xf6, 0xe8);
            theme.chipBorder = QColor(0xd4, 0xa5, 0x74);
            theme.chipText = QColor(0x6b, 0x44, 0x10);
            theme.rowWash = QColor(0xd4, 0xa5, 0x74);
        }
        break;
    case FeatureRunMode::RepeatInfinite:
        if (darkTheme) {
            theme.accent = QColor(0x58, 0xa6, 0xff);
            theme.chipFill = QColor(0x12, 0x1c, 0x2c);
            theme.chipBorder = QColor(0x38, 0x8b, 0xfd);
            theme.chipText = QColor(0x79, 0xc0, 0xff);
            theme.rowWash = QColor(0x58, 0xa6, 0xff);
        } else {
            theme.accent = QColor(0x09, 0x6a, 0xd8);
            theme.chipFill = QColor(0xee, 0xf4, 0xff);
            theme.chipBorder = QColor(0x58, 0xa6, 0xff);
            theme.chipText = QColor(0x05, 0x4f, 0xa8);
            theme.rowWash = QColor(0x58, 0xa6, 0xff);
        }
        break;
    case FeatureRunMode::Trigger:
        if (darkTheme) {
            theme.accent = QColor(0x4e, 0xa8, 0x8c);
            theme.chipFill = QColor(0x14, 0x28, 0x22);
            theme.chipBorder = QColor(0x3a, 0x8a, 0x74);
            theme.chipText = QColor(0x8f, 0xd4, 0xbc);
            theme.rowWash = QColor(0x4e, 0xa8, 0x8c);
        } else {
            theme.accent = QColor(0x1a, 0x7a, 0x62);
            theme.chipFill = QColor(0xec, 0xf8, 0xf4);
            theme.chipBorder = QColor(0x4e, 0xa8, 0x8c);
            theme.chipText = QColor(0x0f, 0x5c, 0x48);
            theme.rowWash = QColor(0x4e, 0xa8, 0x8c);
        }
        break;
    case FeatureRunMode::RepeatCount:
    default:
        if (darkTheme) {
            theme.accent = QColor(0xa3, 0x71, 0xf7);
            theme.chipFill = QColor(0x22, 0x16, 0x30);
            theme.chipBorder = QColor(0x89, 0x57, 0xe5);
            theme.chipText = QColor(0xd2, 0xa8, 0xff);
            theme.rowWash = QColor(0xa3, 0x71, 0xf7);
        } else {
            theme.accent = QColor(0x6e, 0x40, 0xc9);
            theme.chipFill = QColor(0xf6, 0xf0, 0xff);
            theme.chipBorder = QColor(0xa3, 0x71, 0xf7);
            theme.chipText = QColor(0x4a, 0x28, 0x8a);
            theme.rowWash = QColor(0xa3, 0x71, 0xf7);
        }
        break;
    }
    return theme;
}
