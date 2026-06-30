#include "app/PointerFeedbackSettings.h"

#include <QSettings>

#include <algorithm>

namespace {

constexpr const char* kClickPrefix = "program/pointerFeedback/click/";

int clampInt(int value, int minValue, int maxValue) {
    return std::clamp(value, minValue, maxValue);
}

double clampDouble(double value, double minValue, double maxValue) {
    return std::clamp(value, minValue, maxValue);
}

ClickPointerFeedbackShape clampShape(int value) {
    const int maxShape = static_cast<int>(ClickPointerFeedbackShape::Square);
    return static_cast<ClickPointerFeedbackShape>(clampInt(value, 0, maxShape));
}

QColor readColor(const QSettings& settings, const QString& key, const QColor& fallback) {
    const QVariant stored = settings.value(key);
    if (!stored.isValid()) {
        return fallback;
    }
    const QColor parsed(stored.toString());
    return parsed.isValid() ? parsed : fallback;
}

} // namespace

ClickPointerFeedbackSettings PointerFeedbackSettings::defaultClick() {
    return ClickPointerFeedbackSettings{};
}

ClickPointerFeedbackSettings PointerFeedbackSettings::click() {
    const ClickPointerFeedbackSettings defaults = defaultClick();
    QSettings settings;

    ClickPointerFeedbackSettings result = defaults;
    result.displayDurationMs =
        clampInt(settings.value(QString::fromLatin1("%1displayDurationMs").arg(kClickPrefix),
                               defaults.displayDurationMs)
                     .toInt(),
                 100,
                 3000);
    result.animationSpeed =
        clampDouble(settings.value(QString::fromLatin1("%1animationSpeed").arg(kClickPrefix),
                                   defaults.animationSpeed)
                        .toDouble(),
                    0.25,
                    4.0);
    result.shape = clampShape(settings.value(QString::fromLatin1("%1shape").arg(kClickPrefix),
                                             static_cast<int>(defaults.shape))
                                  .toInt());
    result.color = readColor(settings, QString::fromLatin1("%1color").arg(kClickPrefix), defaults.color);
    result.coreSize = clampInt(settings.value(QString::fromLatin1("%1coreSize").arg(kClickPrefix),
                                              defaults.coreSize)
                                   .toInt(),
                               2,
                               16);
    result.maxExpandRadius =
        clampInt(settings.value(QString::fromLatin1("%1maxExpandRadius").arg(kClickPrefix),
                                defaults.maxExpandRadius)
                     .toInt(),
                 8,
                 80);
    result.ringCount = clampInt(settings.value(QString::fromLatin1("%1ringCount").arg(kClickPrefix),
                                               defaults.ringCount)
                                    .toInt(),
                                0,
                                4);
    result.ringThickness =
        clampInt(settings.value(QString::fromLatin1("%1ringThickness").arg(kClickPrefix),
                                defaults.ringThickness)
                     .toInt(),
                 1,
                 8);
    result.maxAlpha = clampInt(settings.value(QString::fromLatin1("%1maxAlpha").arg(kClickPrefix),
                                              defaults.maxAlpha)
                                   .toInt(),
                               40,
                               255);
    return result;
}

void PointerFeedbackSettings::setClick(const ClickPointerFeedbackSettings& settings) {
    const ClickPointerFeedbackSettings clamped = [settings]() {
        ClickPointerFeedbackSettings s = settings;
        s.displayDurationMs = clampInt(s.displayDurationMs, 100, 3000);
        s.animationSpeed = clampDouble(s.animationSpeed, 0.25, 4.0);
        s.shape = clampShape(static_cast<int>(s.shape));
        if (!s.color.isValid()) {
            s.color = defaultClick().color;
        }
        s.coreSize = clampInt(s.coreSize, 2, 16);
        s.maxExpandRadius = clampInt(s.maxExpandRadius, 8, 80);
        s.ringCount = clampInt(s.ringCount, 0, 4);
        s.ringThickness = clampInt(s.ringThickness, 1, 8);
        s.maxAlpha = clampInt(s.maxAlpha, 40, 255);
        return s;
    }();

    QSettings qsettings;
    qsettings.setValue(QString::fromLatin1("%1displayDurationMs").arg(kClickPrefix), clamped.displayDurationMs);
    qsettings.setValue(QString::fromLatin1("%1animationSpeed").arg(kClickPrefix), clamped.animationSpeed);
    qsettings.setValue(QString::fromLatin1("%1shape").arg(kClickPrefix), static_cast<int>(clamped.shape));
    qsettings.setValue(QString::fromLatin1("%1color").arg(kClickPrefix), clamped.color.name(QColor::HexRgb));
    qsettings.setValue(QString::fromLatin1("%1coreSize").arg(kClickPrefix), clamped.coreSize);
    qsettings.setValue(QString::fromLatin1("%1maxExpandRadius").arg(kClickPrefix), clamped.maxExpandRadius);
    qsettings.setValue(QString::fromLatin1("%1ringCount").arg(kClickPrefix), clamped.ringCount);
    qsettings.setValue(QString::fromLatin1("%1ringThickness").arg(kClickPrefix), clamped.ringThickness);
    qsettings.setValue(QString::fromLatin1("%1maxAlpha").arg(kClickPrefix), clamped.maxAlpha);
}
