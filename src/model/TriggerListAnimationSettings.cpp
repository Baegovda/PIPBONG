#include "model/TriggerListAnimationSettings.h"

#include "ui/FeatureRunModeTheme.h"

#include <QObject>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

const QColor kDefaultTriggerWatchListAccent = featureRunModeTriggerWatchAccent(true);
const QColor kDefaultTriggerCooldownListAccent = featureRunModeTriggerCooldownAccent(true);

namespace {

constexpr double kMinTriggerAnimationSpeed = 0.25;
constexpr double kMaxTriggerAnimationSpeed = 3.0;

double clampSpeed(double speed) {
    return std::clamp(speed, kMinTriggerAnimationSpeed, kMaxTriggerAnimationSpeed);
}

TriggerListAnimationStyle styleFromString(const std::string& value, TriggerListAnimationStyle fallback) {
    if (value == "SonarRadar") {
        return TriggerListAnimationStyle::SonarRadar;
    }
    if (value == "StaticBullseye") {
        return TriggerListAnimationStyle::StaticBullseye;
    }
    if (value == "SpinningArc") {
        return TriggerListAnimationStyle::SpinningArc;
    }
    if (value == "PulseRing") {
        return TriggerListAnimationStyle::PulseRing;
    }
    if (value == "CountdownText") {
        return TriggerListAnimationStyle::CountdownText;
    }
    if (value == "DefaultPrism") {
        return TriggerListAnimationStyle::DefaultPrism;
    }
    return fallback;
}

std::string styleToString(TriggerListAnimationStyle style) {
    switch (style) {
    case TriggerListAnimationStyle::SonarRadar:
        return "SonarRadar";
    case TriggerListAnimationStyle::StaticBullseye:
        return "StaticBullseye";
    case TriggerListAnimationStyle::SpinningArc:
        return "SpinningArc";
    case TriggerListAnimationStyle::PulseRing:
        return "PulseRing";
    case TriggerListAnimationStyle::CountdownText:
        return "CountdownText";
    case TriggerListAnimationStyle::DefaultPrism:
        return "DefaultPrism";
    }
    return "SonarRadar";
}

QColor readJsonColor(const nlohmann::json& json, const char* key) {
    if (!json.contains(key)) {
        return {};
    }
    const QColor parsed(QString::fromStdString(json[key].get<std::string>()));
    return parsed.isValid() ? parsed : QColor();
}

void writeOptionalColor(nlohmann::json& json, const char* key, const QColor& color) {
    if (color.isValid()) {
        json[key] = color.name(QColor::HexRgb).toStdString();
    }
}

TriggerListStateAnimationSettings stateFromJson(const nlohmann::json& json,
                                                const TriggerListStateAnimationSettings& defaults) {
    TriggerListStateAnimationSettings settings = defaults;
    if (json.contains("style")) {
        settings.style = styleFromString(json["style"].get<std::string>(), defaults.style);
    }
    settings.accentColor = readJsonColor(json, "accentColor");
    if (json.contains("speed")) {
        settings.speed = clampSpeed(json["speed"].get<double>());
    }
    if (json.contains("showSonarRipples")) {
        settings.showSonarRipples = json["showSonarRipples"].get<bool>();
    }
    if (json.contains("showRadarSweep")) {
        settings.showRadarSweep = json["showRadarSweep"].get<bool>();
    }
    return settings;
}

nlohmann::json stateToJson(const TriggerListStateAnimationSettings& settings,
                           const TriggerListStateAnimationSettings& defaults) {
    nlohmann::json json;
    if (settings.style != defaults.style) {
        json["style"] = styleToString(settings.style);
    }
    writeOptionalColor(json, "accentColor", settings.accentColor);
    if (std::abs(settings.speed - defaults.speed) > 0.001) {
        json["speed"] = settings.speed;
    }
    if (settings.showSonarRipples != defaults.showSonarRipples) {
        json["showSonarRipples"] = settings.showSonarRipples;
    }
    if (settings.showRadarSweep != defaults.showRadarSweep) {
        json["showRadarSweep"] = settings.showRadarSweep;
    }
    return json;
}

bool stateEquals(const TriggerListStateAnimationSettings& a,
                 const TriggerListStateAnimationSettings& b) {
    return a.style == b.style && a.accentColor == b.accentColor
           && std::abs(a.speed - b.speed) < 0.001 && a.showSonarRipples == b.showSonarRipples
           && a.showRadarSweep == b.showRadarSweep;
}

} // namespace

TriggerListStateAnimationSettings TriggerListStateAnimationSettings::watchDefaults() {
    TriggerListStateAnimationSettings settings;
    settings.style = TriggerListAnimationStyle::SonarRadar;
    return settings;
}

TriggerListStateAnimationSettings TriggerListStateAnimationSettings::cooldownDefaults() {
    TriggerListStateAnimationSettings settings;
    settings.style = TriggerListAnimationStyle::CountdownText;
    return settings;
}

TriggerListStateAnimationSettings TriggerListStateAnimationSettings::actionDefaults() {
    TriggerListStateAnimationSettings settings;
    settings.style = TriggerListAnimationStyle::DefaultPrism;
    return settings;
}

TriggerModeListAnimationSettings TriggerModeListAnimationSettings::defaults() {
    TriggerModeListAnimationSettings settings;
    settings.watch = TriggerListStateAnimationSettings::watchDefaults();
    settings.cooldown = TriggerListStateAnimationSettings::cooldownDefaults();
    settings.action = TriggerListStateAnimationSettings::actionDefaults();
    return settings;
}

bool TriggerModeListAnimationSettings::isDefault() const {
    return *this == defaults();
}

QString triggerListAnimationStyleDisplayName(TriggerListAnimationStyle style) {
    switch (style) {
    case TriggerListAnimationStyle::SonarRadar:
        return QObject::tr("소나 레이더");
    case TriggerListAnimationStyle::StaticBullseye:
        return QObject::tr("고정 ◎");
    case TriggerListAnimationStyle::SpinningArc:
        return QObject::tr("회전 호");
    case TriggerListAnimationStyle::PulseRing:
        return QObject::tr("링 펄스");
    case TriggerListAnimationStyle::CountdownText:
        return QObject::tr("남은 시간(초)");
    case TriggerListAnimationStyle::DefaultPrism:
        return QObject::tr("기본(프리즘)");
    }
    return QObject::tr("소나 레이더");
}

QString triggerModeListAnimationSummary(const TriggerModeListAnimationSettings& settings) {
    return QObject::tr("감시: %1 · 쿨다운: %2 · 동작: %3")
        .arg(triggerListAnimationStyleDisplayName(settings.watch.style),
             triggerListAnimationStyleDisplayName(settings.cooldown.style),
             triggerListAnimationStyleDisplayName(settings.action.style));
}

TriggerListStateAnimationSettings clampTriggerListStateAnimation(
    const TriggerListStateAnimationSettings& settings,
    TriggerAnimationState state) {
    TriggerListStateAnimationSettings clamped = settings;
    clamped.speed = clampSpeed(clamped.speed);

    switch (state) {
    case TriggerAnimationState::Watch:
        if (clamped.style == TriggerListAnimationStyle::CountdownText
            || clamped.style == TriggerListAnimationStyle::DefaultPrism) {
            clamped.style = TriggerListAnimationStyle::SonarRadar;
        }
        break;
    case TriggerAnimationState::Cooldown:
        if (clamped.style == TriggerListAnimationStyle::DefaultPrism) {
            clamped.style = TriggerListAnimationStyle::CountdownText;
        }
        break;
    case TriggerAnimationState::Action:
        if (clamped.style == TriggerListAnimationStyle::CountdownText) {
            clamped.style = TriggerListAnimationStyle::DefaultPrism;
        }
        break;
    }
    return clamped;
}

TriggerModeListAnimationSettings clampTriggerModeListAnimations(
    const TriggerModeListAnimationSettings& settings) {
    TriggerModeListAnimationSettings clamped = settings;
    clamped.watch = clampTriggerListStateAnimation(clamped.watch, TriggerAnimationState::Watch);
    clamped.cooldown =
        clampTriggerListStateAnimation(clamped.cooldown, TriggerAnimationState::Cooldown);
    clamped.action = clampTriggerListStateAnimation(clamped.action, TriggerAnimationState::Action);
    return clamped;
}

TriggerModeListAnimationSettings triggerModeListAnimationsFromJson(const nlohmann::json& json) {
    const auto defaults = TriggerModeListAnimationSettings::defaults();
    TriggerModeListAnimationSettings settings = defaults;
    if (json.contains("watch")) {
        settings.watch = stateFromJson(json["watch"], defaults.watch);
    }
    if (json.contains("cooldown")) {
        settings.cooldown = stateFromJson(json["cooldown"], defaults.cooldown);
    }
    if (json.contains("action")) {
        settings.action = stateFromJson(json["action"], defaults.action);
    }
    return clampTriggerModeListAnimations(settings);
}

nlohmann::json triggerModeListAnimationsToJson(const TriggerModeListAnimationSettings& settings) {
    const auto defaults = TriggerModeListAnimationSettings::defaults();
    nlohmann::json json;
    const nlohmann::json watchJson = stateToJson(settings.watch, defaults.watch);
    const nlohmann::json cooldownJson = stateToJson(settings.cooldown, defaults.cooldown);
    const nlohmann::json actionJson = stateToJson(settings.action, defaults.action);
    if (!watchJson.empty()) {
        json["watch"] = watchJson;
    }
    if (!cooldownJson.empty()) {
        json["cooldown"] = cooldownJson;
    }
    if (!actionJson.empty()) {
        json["action"] = actionJson;
    }
    return json;
}

bool operator==(const TriggerListStateAnimationSettings& a, const TriggerListStateAnimationSettings& b) {
    return stateEquals(a, b);
}

bool operator==(const TriggerModeListAnimationSettings& a, const TriggerModeListAnimationSettings& b) {
    return stateEquals(a.watch, b.watch) && stateEquals(a.cooldown, b.cooldown)
           && stateEquals(a.action, b.action);
}
