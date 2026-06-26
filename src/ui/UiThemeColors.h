#pragma once

#include <QColor>
#include <QLabel>
#include <QPalette>
#include <QString>
#include <QWidget>

inline bool isDarkTheme(const QPalette& pal) {
    const QColor window = pal.color(QPalette::Window);
    if (window.isValid()) {
        return window.lightness() < 128;
    }
    return pal.color(QPalette::WindowText).lightness() > 128;
}

/// Secondary hint/caption text — readable on both light and dark themes.
inline QColor secondaryHintTextColor(const QPalette& pal) {
    if (isDarkTheme(pal)) {
        return QColor(0xa8, 0xd4, 0xff);
    }
    return QColor(0x3d, 0x5a, 0x72);
}

/// Warning/info note text (e.g. block type change notice).
inline QColor warningNoteTextColor(const QPalette& pal) {
    if (isDarkTheme(pal)) {
        return QColor(0xff, 0xcc, 0x66);
    }
    return QColor(0xa0, 0x60, 0x00);
}

inline void applyLabelTextColor(QLabel* label, const QColor& color) {
    if (!label) {
        return;
    }
    QPalette labelPalette = label->palette();
    labelPalette.setColor(QPalette::WindowText, color);
    labelPalette.setColor(QPalette::Text, color);
    label->setPalette(labelPalette);
}

inline QString hotkeyBindingLabelIdleStyleSheet(const QPalette& pal) {
    if (isDarkTheme(pal)) {
        return QStringLiteral(
            "font-weight: bold; padding: 8px; border: 1px solid #6a7888; border-radius: 4px; "
            "background-color: #2a323c; color: #e8eef4;");
    }
    return QStringLiteral(
        "font-weight: bold; padding: 8px; border: 1px solid #bbb; border-radius: 4px; "
        "background-color: #ffffff; color: #202020;");
}

inline QString hotkeyBindingLabelCaptureStyleSheet(const QPalette& pal) {
    if (isDarkTheme(pal)) {
        return QStringLiteral(
            "font-weight: bold; padding: 8px; border: 2px solid #6a9fd8; border-radius: 4px; "
            "background-color: #1e3a52; color: #d8ebff;");
    }
    return QStringLiteral(
        "font-weight: bold; padding: 8px; border: 2px solid #6a9fd8; border-radius: 4px; "
        "background-color: #eef4fb; color: #202020;");
}

inline QString hotkeyBindingLabelLargeIdleStyleSheet(const QPalette& pal) {
    if (isDarkTheme(pal)) {
        return QStringLiteral(
            "font-size: 16px; font-weight: bold; padding: 12px; border: 1px solid #6a7888; "
            "border-radius: 4px; background-color: #2a323c; color: #e8eef4;");
    }
    return QStringLiteral(
        "font-size: 16px; font-weight: bold; padding: 12px; border: 1px solid #bbb; "
        "border-radius: 4px; background-color: #ffffff; color: #202020;");
}

inline QString hotkeyBindingLabelLargeCaptureStyleSheet(const QPalette& pal) {
    if (isDarkTheme(pal)) {
        return QStringLiteral(
            "font-size: 16px; font-weight: bold; padding: 12px; border: 2px solid #6a9fd8; "
            "border-radius: 4px; background-color: #1e3a52; color: #d8ebff;");
    }
    return QStringLiteral(
        "font-size: 16px; font-weight: bold; padding: 12px; border: 2px solid #6a9fd8; "
        "border-radius: 4px; background-color: #eef4fb; color: #202020;");
}
