#pragma once

#include <QColor>
#include <QString>

#include <nlohmann/json_fwd.hpp>

enum class TriggerListAnimationStyle {
    SonarRadar = 0,
    StaticBullseye,
    SpinningArc,
    PulseRing,
    CountdownText,
    DefaultPrism,
};

enum class TriggerAnimationState {
    Watch = 0,
    Cooldown,
    Action,
};

struct TriggerListStateAnimationSettings {
    TriggerListAnimationStyle style = TriggerListAnimationStyle::SonarRadar;
    QColor accentColor;
    double speed = 1.0;
    bool showSonarRipples = true;
    bool showRadarSweep = true;

    static TriggerListStateAnimationSettings watchDefaults();
    static TriggerListStateAnimationSettings cooldownDefaults();
    static TriggerListStateAnimationSettings actionDefaults();
};

struct TriggerModeListAnimationSettings {
    TriggerListStateAnimationSettings watch;
    TriggerListStateAnimationSettings cooldown;
    TriggerListStateAnimationSettings action;

    static TriggerModeListAnimationSettings defaults();
    bool isDefault() const;
};

extern const QColor kDefaultTriggerWatchListAccent;
extern const QColor kDefaultTriggerCooldownListAccent;

QString triggerListAnimationStyleDisplayName(TriggerListAnimationStyle style);
QString triggerModeListAnimationSummary(const TriggerModeListAnimationSettings& settings);

TriggerListStateAnimationSettings clampTriggerListStateAnimation(
    const TriggerListStateAnimationSettings& settings,
    TriggerAnimationState state);
TriggerModeListAnimationSettings clampTriggerModeListAnimations(
    const TriggerModeListAnimationSettings& settings);
TriggerModeListAnimationSettings triggerModeListAnimationsFromJson(const nlohmann::json& json);
nlohmann::json triggerModeListAnimationsToJson(const TriggerModeListAnimationSettings& settings);

bool operator==(const TriggerListStateAnimationSettings& a, const TriggerListStateAnimationSettings& b);
bool operator==(const TriggerModeListAnimationSettings& a, const TriggerModeListAnimationSettings& b);
