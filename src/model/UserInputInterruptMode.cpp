#include "model/UserInputInterruptMode.h"

std::string userInputInterruptModeToString(UserInputInterruptMode mode) {
    switch (mode) {
    case UserInputInterruptMode::Pause:
        return "Pause";
    case UserInputInterruptMode::None:
        return "None";
    case UserInputInterruptMode::Stop:
        return "Stop";
    }
    return "Stop";
}

UserInputInterruptMode userInputInterruptModeFromString(const std::string& value) {
    if (value == "Pause") {
        return UserInputInterruptMode::Pause;
    }
    if (value == "None") {
        return UserInputInterruptMode::None;
    }
    return UserInputInterruptMode::Stop;
}
