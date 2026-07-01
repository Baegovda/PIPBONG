#pragma once

#include <string>

enum class UserInputInterruptMode {
    Pause,
    Stop
};

std::string userInputInterruptModeToString(UserInputInterruptMode mode);
UserInputInterruptMode userInputInterruptModeFromString(const std::string& value);
