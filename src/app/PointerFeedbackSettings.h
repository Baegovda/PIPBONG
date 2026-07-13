#pragma once

#include <QColor>

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
    int coreSize = 4;
    int maxExpandRadius = 26;
    int ringCount = 2;
    int ringThickness = 3;
    int maxAlpha = 220;
};

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

class PointerFeedbackSettings {
public:
    static ClickPointerFeedbackSettings click();
    static void setClick(const ClickPointerFeedbackSettings& settings);
    static ClickPointerFeedbackSettings defaultClick();

    static WindowSelectionFeedbackSettings windowSelection();
    static void setWindowSelection(const WindowSelectionFeedbackSettings& settings);
    static WindowSelectionFeedbackSettings defaultWindowSelection();
};
