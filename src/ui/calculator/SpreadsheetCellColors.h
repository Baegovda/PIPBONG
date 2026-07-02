#pragma once

#include <QColor>
#include <QVector>

inline QVector<QColor> spreadsheetBackgroundPalette() {
    return {
        QColor(255, 255, 255),
        QColor(242, 242, 242),
        QColor(255, 242, 204),
        QColor(226, 239, 218),
        QColor(204, 229, 255),
        QColor(255, 204, 204),
        QColor(255, 230, 153),
        QColor(228, 208, 255),
        QColor(255, 192, 0),
        QColor(146, 208, 80),
        QColor(91, 155, 213),
        QColor(237, 125, 49),
    };
}

inline QVector<QColor> spreadsheetForegroundPalette() {
    return {
        QColor(0, 0, 0),
        QColor(68, 68, 68),
        QColor(192, 0, 0),
        QColor(0, 97, 0),
        QColor(0, 112, 192),
        QColor(112, 48, 160),
        QColor(237, 125, 49),
        QColor(255, 255, 255),
    };
}

inline QString colorToJsonHex(const QColor& color) {
    if (!color.isValid()) {
        return {};
    }
    return color.name(QColor::HexRgb);
}

inline QColor colorFromJsonHex(const QString& hex) {
    if (hex.isEmpty()) {
        return {};
    }
    QColor color(hex);
    return color.isValid() ? color : QColor{};
}
