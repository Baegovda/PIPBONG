#pragma once

#include <string>

enum class UserInputInterruptMode {
    Pause,
    Stop,
    None
};

std::string userInputInterruptModeToString(UserInputInterruptMode mode);
UserInputInterruptMode userInputInterruptModeFromString(const std::string& value);
