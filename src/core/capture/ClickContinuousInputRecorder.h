#pragma once

#include <functional>
#include <memory>

class QWidget;
class ClickBlock;

/// Low-level mouse hook session for ClickEditor continuous fixed-coordinate input.
class ClickContinuousInputRecorder {
public:
    using BlockFactory = std::function<std::unique_ptr<ClickBlock>(int x, int y)>;
    using ClickCallback = std::function<void(std::unique_ptr<ClickBlock>)>;
    using ArmedCallback = std::function<void(bool armed)>;

    static void beginSession(QWidget* hostWidget,
                             bool useClientCoordinates,
                             BlockFactory factory,
                             ClickCallback onCaptured,
                             ArmedCallback onArmedChanged);
    static void endSession();
    static bool isSessionActive();
    static bool isArmed();
    static void setArmed(bool armed);
    static bool isHostActive(QWidget* hostWidget);
};
