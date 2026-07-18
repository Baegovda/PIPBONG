#pragma once

#include <QColor>
#include <QMetaType>

#include <nlohmann/json_fwd.hpp>

/// Global click pointer-feedback animation preferences (QSettings, not project JSON).
enum class ClickPointerFeedbackShape {
    FilledDotRings = 0,
    RingOnly,
    Crosshair,
    Square,
};

struct ClickPointerFeedbackSettings {
    int displayDurationMs = 460;
    double animationSpeed = 1.0;
    ClickPointerFeedbackShape shape = ClickPointerFeedbackShape::FilledDotRings;
    QColor color{74, 160, 255};
    /// Optional per-part colors for click feedback; invalid → use `color`.
    QColor coreColor;
    QColor ringColor;
    /// Per-part colors for template-match detection feedback (success / miss).
    QColor successCoreColor{90, 240, 150};
    QColor successRingColor{90, 240, 150};
    QColor missCoreColor{255, 110, 120};
    QColor missRingColor{255, 110, 120};
    int coreSize = 4;
    int maxExpandRadius = 26;
    int ringCount = 2;
    int ringThickness = 3;
    int maxAlpha = 220;
};

QColor resolvedClickCoreColor(const ClickPointerFeedbackSettings& settings);
QColor resolvedClickRingColor(const ClickPointerFeedbackSettings& settings);
QColor detectionCoreColor(const ClickPointerFeedbackSettings& settings, bool success);
QColor detectionRingColor(const ClickPointerFeedbackSettings& settings, bool success);

/// Global window-pick confirmation overlay animation (QSettings, not project JSON).
enum class WindowSelectionFeedbackStyle {
    LockOn = 0,
    RadarPing,
    BorderGlow,
    ClassicFill,
};

struct WindowSelectionFeedbackSettings {
    bool enabled = true;
    int displayDurationMs = 1000;
    double animationSpeed = 1.0;
    WindowSelectionFeedbackStyle style = WindowSelectionFeedbackStyle::LockOn;
    QColor color{56, 189, 248};
    int maxAlpha = 220;
    int pingRingWidth = 18;
    int edgeGlowWidth = 16;
    int bracketScalePercent = 100;
    bool echoRing = true;
    bool centerBloom = true;
    bool edgeGlow = true;
    bool cornerBrackets = true;
};

ClickPointerFeedbackSettings clampClickPointerFeedback(const ClickPointerFeedbackSettings& settings);
ClickPointerFeedbackSettings clickPointerFeedbackFromJson(const nlohmann::json& json);
nlohmann::json clickPointerFeedbackToJson(const ClickPointerFeedbackSettings& settings);

class PointerFeedbackSettings {
public:
    static ClickPointerFeedbackSettings click();
    static void setClick(const ClickPointerFeedbackSettings& settings);
    static ClickPointerFeedbackSettings defaultClick();

    static WindowSelectionFeedbackSettings windowSelection();
    static void setWindowSelection(const WindowSelectionFeedbackSettings& settings);
    static WindowSelectionFeedbackSettings defaultWindowSelection();
};

Q_DECLARE_METATYPE(ClickPointerFeedbackSettings)
