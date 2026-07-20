#pragma once

#include "model/FeatureRunMode.h"

#include <QColor>
#include <QString>

class QComboBox;
class QFont;
class QLabel;
class QPainter;
class QPalette;
class QRect;

struct FeatureRunModeTheme {
    QColor accent;
    QColor chipFill;
    QColor chipBorder;
    QColor chipText;
    QColor rowWash;
};

FeatureRunModeTheme featureRunModeTheme(FeatureRunMode mode, bool darkTheme);

bool featureRunModeDarkThemeFromPalette(const QPalette& palette);

QString featureRunModeLabel(FeatureRunMode mode);
QString featureRunModeCompact(FeatureRunMode mode, int repeatCount);

QString featureRunModeChipStyleSheet(FeatureRunMode mode,
                                     bool darkTheme,
                                     const QString& selector);

QColor featureRunModeTriggerWatchWash(bool darkTheme);
QColor featureRunModeTriggerCooldownWash(bool darkTheme);
QColor featureRunModeTriggerWatchAccent(bool darkTheme);
QColor featureRunModeTriggerCooldownAccent(bool darkTheme);

void applyFeatureRunModeChipLabel(QLabel* label, FeatureRunMode mode, int repeatCount = 1);
void applyFeatureRunModeComboAccent(QComboBox* combo, FeatureRunMode mode);

void paintFeatureRunModeChip(QPainter* painter,
                             const QRect& columnRect,
                             const QString& text,
                             const QFont& font,
                             FeatureRunMode mode,
                             const QPalette& palette,
                             bool enabled,
                             const QColor* accentOverride = nullptr);
