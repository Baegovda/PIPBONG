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

class PointerFeedbackSettings {
public:
    static ClickPointerFeedbackSettings click();
    static void setClick(const ClickPointerFeedbackSettings& settings);
    static ClickPointerFeedbackSettings defaultClick();
};
