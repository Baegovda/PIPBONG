#include "app/PointerFeedbackSettings.h"

#include <QColor>
#include <QSettings>

#include <nlohmann/json.hpp>

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

QColor readOptionalColor(const QSettings& settings, const QString& key) {
    const QVariant stored = settings.value(key);
    if (!stored.isValid()) {
        return {};
    }
    const QColor parsed(stored.toString());
    return parsed.isValid() ? parsed : QColor();
}

void writeOptionalColor(QSettings& settings, const QString& key, const QColor& color) {
    if (color.isValid()) {
        settings.setValue(key, color.name(QColor::HexRgb));
    } else {
        settings.remove(key);
    }
}

QColor readJsonColor(const nlohmann::json& json, const char* key, const QColor& fallback) {
    if (!json.contains(key)) {
        return fallback;
    }
    const QColor parsed(QString::fromStdString(json[key].get<std::string>()));
    return parsed.isValid() ? parsed : fallback;
}

} // namespace

QColor resolvedClickCoreColor(const ClickPointerFeedbackSettings& settings) {
    return settings.coreColor.isValid() ? settings.coreColor : settings.color;
}

QColor resolvedClickRingColor(const ClickPointerFeedbackSettings& settings) {
    return settings.ringColor.isValid() ? settings.ringColor : settings.color;
}

QColor detectionCoreColor(const ClickPointerFeedbackSettings& settings, bool success) {
    return success ? settings.successCoreColor : settings.missCoreColor;
}

QColor detectionRingColor(const ClickPointerFeedbackSettings& settings, bool success) {
    return success ? settings.successRingColor : settings.missRingColor;
}

ClickPointerFeedbackSettings PointerFeedbackSettings::defaultClick() {
    return ClickPointerFeedbackSettings{};
}

ClickPointerFeedbackSettings clampClickPointerFeedback(const ClickPointerFeedbackSettings& settings) {
    const ClickPointerFeedbackSettings defaults = PointerFeedbackSettings::defaultClick();
    ClickPointerFeedbackSettings result = settings;
    result.displayDurationMs = clampInt(result.displayDurationMs, 100, 3000);
    result.animationSpeed = clampDouble(result.animationSpeed, 0.25, 4.0);
    result.shape = clampShape(static_cast<int>(result.shape));
    if (!result.color.isValid()) {
        result.color = defaults.color;
    }
    if (!result.successCoreColor.isValid()) {
        result.successCoreColor = defaults.successCoreColor;
    }
    if (!result.successRingColor.isValid()) {
        result.successRingColor = defaults.successRingColor;
    }
    if (!result.missCoreColor.isValid()) {
        result.missCoreColor = defaults.missCoreColor;
    }
    if (!result.missRingColor.isValid()) {
        result.missRingColor = defaults.missRingColor;
    }
    result.coreSize = clampInt(result.coreSize, 2, 16);
    result.maxExpandRadius = clampInt(result.maxExpandRadius, 8, 80);
    result.ringCount = clampInt(result.ringCount, 0, 4);
    result.ringThickness = clampInt(result.ringThickness, 1, 8);
    result.maxAlpha = clampInt(result.maxAlpha, 40, 255);
    return result;
}

ClickPointerFeedbackSettings clickPointerFeedbackFromJson(const nlohmann::json& json) {
    const ClickPointerFeedbackSettings defaults = PointerFeedbackSettings::defaultClick();
    ClickPointerFeedbackSettings result = defaults;
    if (json.contains("displayDurationMs")) {
        result.displayDurationMs = json["displayDurationMs"].get<int>();
    }
    if (json.contains("animationSpeed")) {
        result.animationSpeed = json["animationSpeed"].get<double>();
    }
    if (json.contains("shape")) {
        result.shape = clampShape(json["shape"].get<int>());
    }
    if (json.contains("color")) {
        result.color = readJsonColor(json, "color", defaults.color);
    }
    if (json.contains("coreColor")) {
        result.coreColor = readJsonColor(json, "coreColor", {});
    }
    if (json.contains("ringColor")) {
        result.ringColor = readJsonColor(json, "ringColor", {});
    }
    if (json.contains("successCoreColor")) {
        result.successCoreColor = readJsonColor(json, "successCoreColor", defaults.successCoreColor);
    }
    if (json.contains("successRingColor")) {
        result.successRingColor = readJsonColor(json, "successRingColor", defaults.successRingColor);
    }
    if (json.contains("missCoreColor")) {
        result.missCoreColor = readJsonColor(json, "missCoreColor", defaults.missCoreColor);
    }
    if (json.contains("missRingColor")) {
        result.missRingColor = readJsonColor(json, "missRingColor", defaults.missRingColor);
    }
    if (json.contains("coreSize")) {
        result.coreSize = json["coreSize"].get<int>();
    }
    if (json.contains("maxExpandRadius")) {
        result.maxExpandRadius = json["maxExpandRadius"].get<int>();
    }
    if (json.contains("ringCount")) {
        result.ringCount = json["ringCount"].get<int>();
    }
    if (json.contains("ringThickness")) {
        result.ringThickness = json["ringThickness"].get<int>();
    }
    if (json.contains("maxAlpha")) {
        result.maxAlpha = json["maxAlpha"].get<int>();
    }
    return clampClickPointerFeedback(result);
}

nlohmann::json clickPointerFeedbackToJson(const ClickPointerFeedbackSettings& settings) {
    const ClickPointerFeedbackSettings clamped = clampClickPointerFeedback(settings);
    nlohmann::json json{
        {"displayDurationMs", clamped.displayDurationMs},
        {"animationSpeed", clamped.animationSpeed},
        {"shape", static_cast<int>(clamped.shape)},
        {"color", clamped.color.name(QColor::HexRgb).toStdString()},
        {"coreSize", clamped.coreSize},
        {"maxExpandRadius", clamped.maxExpandRadius},
        {"ringCount", clamped.ringCount},
        {"ringThickness", clamped.ringThickness},
        {"maxAlpha", clamped.maxAlpha},
        {"successCoreColor", clamped.successCoreColor.name(QColor::HexRgb).toStdString()},
        {"successRingColor", clamped.successRingColor.name(QColor::HexRgb).toStdString()},
        {"missCoreColor", clamped.missCoreColor.name(QColor::HexRgb).toStdString()},
        {"missRingColor", clamped.missRingColor.name(QColor::HexRgb).toStdString()},
    };
    if (clamped.coreColor.isValid()) {
        json["coreColor"] = clamped.coreColor.name(QColor::HexRgb).toStdString();
    }
    if (clamped.ringColor.isValid()) {
        json["ringColor"] = clamped.ringColor.name(QColor::HexRgb).toStdString();
    }
    return json;
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
    result.coreColor = readOptionalColor(settings, QString::fromLatin1("%1coreColor").arg(kClickPrefix));
    result.ringColor = readOptionalColor(settings, QString::fromLatin1("%1ringColor").arg(kClickPrefix));
    result.successCoreColor =
        readColor(settings,
                  QString::fromLatin1("%1successCoreColor").arg(kClickPrefix),
                  defaults.successCoreColor);
    result.successRingColor =
        readColor(settings,
                  QString::fromLatin1("%1successRingColor").arg(kClickPrefix),
                  defaults.successRingColor);
    result.missCoreColor =
        readColor(settings,
                  QString::fromLatin1("%1missCoreColor").arg(kClickPrefix),
                  defaults.missCoreColor);
    result.missRingColor =
        readColor(settings,
                  QString::fromLatin1("%1missRingColor").arg(kClickPrefix),
                  defaults.missRingColor);
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
    const ClickPointerFeedbackSettings clamped = clampClickPointerFeedback(settings);

    QSettings qsettings;
    qsettings.setValue(QString::fromLatin1("%1displayDurationMs").arg(kClickPrefix), clamped.displayDurationMs);
    qsettings.setValue(QString::fromLatin1("%1animationSpeed").arg(kClickPrefix), clamped.animationSpeed);
    qsettings.setValue(QString::fromLatin1("%1shape").arg(kClickPrefix), static_cast<int>(clamped.shape));
    qsettings.setValue(QString::fromLatin1("%1color").arg(kClickPrefix), clamped.color.name(QColor::HexRgb));
    writeOptionalColor(qsettings, QString::fromLatin1("%1coreColor").arg(kClickPrefix), clamped.coreColor);
    writeOptionalColor(qsettings, QString::fromLatin1("%1ringColor").arg(kClickPrefix), clamped.ringColor);
    qsettings.setValue(QString::fromLatin1("%1successCoreColor").arg(kClickPrefix),
                       clamped.successCoreColor.name(QColor::HexRgb));
    qsettings.setValue(QString::fromLatin1("%1successRingColor").arg(kClickPrefix),
                       clamped.successRingColor.name(QColor::HexRgb));
    qsettings.setValue(QString::fromLatin1("%1missCoreColor").arg(kClickPrefix),
                       clamped.missCoreColor.name(QColor::HexRgb));
    qsettings.setValue(QString::fromLatin1("%1missRingColor").arg(kClickPrefix),
                       clamped.missRingColor.name(QColor::HexRgb));
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
