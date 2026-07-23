#include "ui/FeatureRunModeTheme.h"

#include "ui/UiThemeColors.h"

#include <QComboBox>
#include <QLabel>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QWidget>

FeatureRunModeTheme featureRunModeTheme(FeatureRunMode mode, bool darkTheme) {
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

bool featureRunModeDarkThemeFromPalette(const QPalette& palette) {
    const QColor window = palette.color(QPalette::Window);
    if (window.isValid()) {
        return window.lightness() < 128;
    }
    return palette.color(QPalette::WindowText).lightness() > 128;
}

QString featureRunModeLabel(FeatureRunMode mode) {
    switch (mode) {
    case FeatureRunMode::Hold:
        return QObject::tr("홀드");
    case FeatureRunMode::RepeatInfinite:
        return QObject::tr("무한");
    case FeatureRunMode::Trigger:
        return QObject::tr("트리거");
    case FeatureRunMode::RepeatCount:
        return QObject::tr("N회");
    }
    return QObject::tr("N회");
}

QString featureRunModeCompact(FeatureRunMode mode, int repeatCount) {
    switch (mode) {
    case FeatureRunMode::Hold:
        return QObject::tr("홀드");
    case FeatureRunMode::RepeatInfinite:
        return QStringLiteral("\u221e");
    case FeatureRunMode::Trigger:
        return QObject::tr("T");
    case FeatureRunMode::RepeatCount:
        return repeatCount <= 1 ? QStringLiteral("\u00d71") : QStringLiteral("\u00d7%1").arg(repeatCount);
    }
    return QStringLiteral("\u00d71");
}

QString featureRunModeChipStyleSheet(FeatureRunMode mode,
                                     bool darkTheme,
                                     const QString& selector) {
    const FeatureRunModeTheme theme = featureRunModeTheme(mode, darkTheme);
    const QString fill = QStringLiteral("transparent");
    return QStringLiteral(
               "%1 {"
               "  color: %2;"
               "  background-color: %3;"
               "  border: none;"
               "  border-radius: 4px;"
               "  padding: 0 4px;"
               "  font-size: 11px;"
               "  font-weight: 600;"
               "}")
        .arg(selector, theme.chipText.name(), fill);
}

QColor featureRunModeTriggerWatchWash(bool darkTheme) {
    return featureRunModeTheme(FeatureRunMode::Trigger, darkTheme).rowWash;
}

QColor featureRunModeTriggerCooldownWash(bool darkTheme) {
    const FeatureRunModeTheme theme = featureRunModeTheme(FeatureRunMode::Trigger, darkTheme);
    QColor wash = theme.chipBorder;
    if (darkTheme) {
        wash = QColor((wash.red() + 96) / 2, (wash.green() + 100) / 2, (wash.blue() + 108) / 2);
    } else {
        wash = QColor((wash.red() + 180) / 2, (wash.green() + 184) / 2, (wash.blue() + 190) / 2);
    }
    return wash;
}

QColor featureRunModeTriggerWatchAccent(bool darkTheme) {
    return featureRunModeTheme(FeatureRunMode::Trigger, darkTheme).accent;
}

QColor featureRunModeTriggerCooldownAccent(bool darkTheme) {
    const FeatureRunModeTheme theme = featureRunModeTheme(FeatureRunMode::Trigger, darkTheme);
    QColor accent = theme.chipText;
    if (darkTheme) {
        accent = QColor((accent.red() + 118) / 2, (accent.green() + 126) / 2, (accent.blue() + 136) / 2);
    } else {
        accent = theme.chipBorder.darker(115);
    }
    return accent;
}

void applyFeatureRunModeChipLabel(QLabel* label, FeatureRunMode mode, int repeatCount) {
    if (!label) {
        return;
    }
    const bool dark = featureRunModeDarkThemeFromPalette(label->palette());
    label->setText(featureRunModeCompact(mode, repeatCount));
    label->setStyleSheet(featureRunModeChipStyleSheet(mode, dark, QStringLiteral("QLabel")));
}

void applyFeatureRunModeComboAccent(QComboBox* combo, FeatureRunMode mode) {
    if (!combo) {
        return;
    }
    const bool dark = featureRunModeDarkThemeFromPalette(combo->palette());
    const FeatureRunModeTheme theme = featureRunModeTheme(mode, dark);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox {"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  padding: 2px 6px;"
        "  background-color: palette(base);"
        "}"
        "QComboBox:focus {"
        "  border: 1px solid %2;"
        "}"
        "QComboBox:hover {"
        "  border: 1px solid %2;"
        "}")
                             .arg(theme.chipBorder.name(), theme.accent.name()));
}

void paintFeatureRunModeChip(QPainter* painter,
                             const QRect& columnRect,
                             const QString& text,
                             const QFont& font,
                             FeatureRunMode mode,
                             const QPalette& palette,
                             bool enabled,
                             const QColor* accentOverride) {
    const bool dark = featureRunModeDarkThemeFromPalette(palette);
    const FeatureRunModeTheme theme = featureRunModeTheme(mode, dark);

    QColor fill = theme.chipFill;
    QColor textColor = theme.chipText;
    if (accentOverride != nullptr && accentOverride->isValid()) {
        textColor = *accentOverride;
        fill = accentOverride->darker(dark ? 150 : 112);
        fill.setAlpha(dark ? 88 : 210);
    }
    if (!enabled) {
        const QColor muted = secondaryHintTextColor(palette);
        fill = muted;
        fill.setAlpha(dark ? 36 : 52);
        textColor = muted;
    }

    const QFontMetrics metrics(font);
    const int horizontalPad = 4;
    const int chipHeight =
        qBound(14, qMin(columnRect.height() - 4, metrics.height() + 4), columnRect.height());
    const int chipWidth =
        qMin(columnRect.width() - 2, metrics.horizontalAdvance(text) + horizontalPad * 2);
    const QRect chip(columnRect.left() + (columnRect.width() - chipWidth) / 2,
                     columnRect.top() + (columnRect.height() - chipHeight) / 2,
                     chipWidth,
                     chipHeight);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    if (fill.alpha() > 0) {
        painter->setBrush(fill);
        painter->drawRoundedRect(chip, 4, 4);
    }
    painter->setFont(font);
    painter->setPen(textColor);
    painter->drawText(chip, Qt::AlignCenter, text);
    painter->restore();
}
