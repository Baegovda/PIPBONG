#include "app/PointerFeedbackSettings.h"

#include <QColor>
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

namespace {

constexpr const char* kWindowSelectionPrefix = "program/windowSelectionFeedback/";

int clampIntWs(int value, int minValue, int maxValue) {
    return std::clamp(value, minValue, maxValue);
}

double clampDoubleWs(double value, double minValue, double maxValue) {
    return std::clamp(value, minValue, maxValue);
}

WindowSelectionFeedbackStyle clampWindowSelectionStyle(int value) {
    const int maxStyle = static_cast<int>(WindowSelectionFeedbackStyle::ClassicFill);
    return static_cast<WindowSelectionFeedbackStyle>(clampIntWs(value, 0, maxStyle));
}

WindowSelectionFeedbackSettings clampWindowSelection(const WindowSelectionFeedbackSettings& input) {
    WindowSelectionFeedbackSettings s = input;
    s.displayDurationMs = clampIntWs(s.displayDurationMs, 400, 3000);
    s.animationSpeed = clampDoubleWs(s.animationSpeed, 0.25, 4.0);
    s.style = clampWindowSelectionStyle(static_cast<int>(s.style));
    if (!s.color.isValid()) {
        s.color = QColor(56, 189, 248);
    }
    s.maxAlpha = clampIntWs(s.maxAlpha, 40, 255);
    s.pingRingWidth = clampIntWs(s.pingRingWidth, 8, 48);
    s.edgeGlowWidth = clampIntWs(s.edgeGlowWidth, 8, 32);
    s.bracketScalePercent = clampIntWs(s.bracketScalePercent, 50, 150);
    return s;
}

} // namespace

WindowSelectionFeedbackSettings PointerFeedbackSettings::defaultWindowSelection() {
    return WindowSelectionFeedbackSettings{};
}

WindowSelectionFeedbackSettings PointerFeedbackSettings::windowSelection() {
    const WindowSelectionFeedbackSettings defaults = defaultWindowSelection();
    QSettings settings;

    WindowSelectionFeedbackSettings result = defaults;
    result.enabled = settings.value(QString::fromLatin1("%1enabled").arg(kWindowSelectionPrefix), defaults.enabled)
                         .toBool();
    result.displayDurationMs =
        clampIntWs(settings.value(QString::fromLatin1("%1displayDurationMs").arg(kWindowSelectionPrefix),
                                  defaults.displayDurationMs)
                       .toInt(),
                   400, 3000);
    result.animationSpeed =
        clampDoubleWs(settings.value(QString::fromLatin1("%1animationSpeed").arg(kWindowSelectionPrefix),
                                     defaults.animationSpeed)
                          .toDouble(),
                      0.25, 4.0);
    result.style = clampWindowSelectionStyle(
        settings.value(QString::fromLatin1("%1style").arg(kWindowSelectionPrefix),
                       static_cast<int>(defaults.style))
            .toInt());
    result.color = readColor(settings, QString::fromLatin1("%1color").arg(kWindowSelectionPrefix), defaults.color);
    result.maxAlpha = clampIntWs(settings.value(QString::fromLatin1("%1maxAlpha").arg(kWindowSelectionPrefix),
                                                defaults.maxAlpha)
                                     .toInt(),
                                 40, 255);
    result.pingRingWidth =
        clampIntWs(settings.value(QString::fromLatin1("%1pingRingWidth").arg(kWindowSelectionPrefix),
                                  defaults.pingRingWidth)
                       .toInt(),
                   8, 48);
    result.edgeGlowWidth =
        clampIntWs(settings.value(QString::fromLatin1("%1edgeGlowWidth").arg(kWindowSelectionPrefix),
                                  defaults.edgeGlowWidth)
                       .toInt(),
                   8, 32);
    result.bracketScalePercent =
        clampIntWs(settings.value(QString::fromLatin1("%1bracketScalePercent").arg(kWindowSelectionPrefix),
                                  defaults.bracketScalePercent)
                       .toInt(),
                   50, 150);
    result.echoRing = settings.value(QString::fromLatin1("%1echoRing").arg(kWindowSelectionPrefix), defaults.echoRing)
                          .toBool();
    result.centerBloom =
        settings.value(QString::fromLatin1("%1centerBloom").arg(kWindowSelectionPrefix), defaults.centerBloom)
            .toBool();
    result.edgeGlow = settings.value(QString::fromLatin1("%1edgeGlow").arg(kWindowSelectionPrefix), defaults.edgeGlow)
                          .toBool();
    result.cornerBrackets =
        settings.value(QString::fromLatin1("%1cornerBrackets").arg(kWindowSelectionPrefix), defaults.cornerBrackets)
            .toBool();
    return result;
}

void PointerFeedbackSettings::setWindowSelection(const WindowSelectionFeedbackSettings& settings) {
    const WindowSelectionFeedbackSettings clamped = clampWindowSelection(settings);

    QSettings qsettings;
    qsettings.setValue(QString::fromLatin1("%1enabled").arg(kWindowSelectionPrefix), clamped.enabled);
    qsettings.setValue(QString::fromLatin1("%1displayDurationMs").arg(kWindowSelectionPrefix),
                       clamped.displayDurationMs);
    qsettings.setValue(QString::fromLatin1("%1animationSpeed").arg(kWindowSelectionPrefix), clamped.animationSpeed);
    qsettings.setValue(QString::fromLatin1("%1style").arg(kWindowSelectionPrefix), static_cast<int>(clamped.style));
    qsettings.setValue(QString::fromLatin1("%1color").arg(kWindowSelectionPrefix),
                       clamped.color.name(QColor::HexRgb));
    qsettings.setValue(QString::fromLatin1("%1maxAlpha").arg(kWindowSelectionPrefix), clamped.maxAlpha);
    qsettings.setValue(QString::fromLatin1("%1pingRingWidth").arg(kWindowSelectionPrefix), clamped.pingRingWidth);
    qsettings.setValue(QString::fromLatin1("%1edgeGlowWidth").arg(kWindowSelectionPrefix), clamped.edgeGlowWidth);
    qsettings.setValue(QString::fromLatin1("%1bracketScalePercent").arg(kWindowSelectionPrefix),
                       clamped.bracketScalePercent);
    qsettings.setValue(QString::fromLatin1("%1echoRing").arg(kWindowSelectionPrefix), clamped.echoRing);
    qsettings.setValue(QString::fromLatin1("%1centerBloom").arg(kWindowSelectionPrefix), clamped.centerBloom);
    qsettings.setValue(QString::fromLatin1("%1edgeGlow").arg(kWindowSelectionPrefix), clamped.edgeGlow);
    qsettings.setValue(QString::fromLatin1("%1cornerBrackets").arg(kWindowSelectionPrefix), clamped.cornerBrackets);
}
