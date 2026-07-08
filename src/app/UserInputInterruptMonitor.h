#pragma once

#include "core/input/HotkeyBinding.h"
#include "model/UserInputInterruptMode.h"

#include <chrono>
#include <functional>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

class ExecutionContext;

class UserInputInterruptMonitor {
public:
    using InterruptHandler = std::function<void(const std::string& featureId)>;

    static UserInputInterruptMonitor& instance();

    void setHandler(InterruptHandler handler);
    void setHotkeyExemptionCheck(std::function<bool(int virtualKey)> check);

    void registerSession(const std::string& featureId,
                         UserInputInterruptMode mode,
                         const HotkeyBinding& featureHotkey,
                         ExecutionContext* context);
    void unregisterSession(const std::string& featureId);
    void unregisterAll();

    void notifyPhysicalInput(int virtualKey);

private:
    UserInputInterruptMonitor() = default;

    struct SessionEntry {
        UserInputInterruptMode mode = UserInputInterruptMode::Stop;
        HotkeyBinding featureHotkey;
        ExecutionContext* context = nullptr;
        std::chrono::steady_clock::time_point lastInterruptAt{};
    };

    void refreshHooks();

    mutable std::mutex m_mutex;
    InterruptHandler m_handler;
    std::function<bool(int virtualKey)> m_hotkeyExemptionCheck;
    std::unordered_map<std::string, SessionEntry> m_sessions;
};
