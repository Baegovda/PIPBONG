#include "model/UserInputInterruptMode.h"

std::string userInputInterruptModeToString(UserInputInterruptMode mode) {
    switch (mode) {
    case UserInputInterruptMode::Pause:
        return "Pause";
    case UserInputInterruptMode::Stop:
        return "Stop";
    case UserInputInterruptMode::None:
    default:
        return "None";
    }
}

UserInputInterruptMode userInputInterruptModeFromString(const std::string& value) {
    if (value == "Pause") {
        return UserInputInterruptMode::Pause;
    }
    // "None" (legacy) and omitted JSON default to Stop.
    return UserInputInterruptMode::Stop;
}
