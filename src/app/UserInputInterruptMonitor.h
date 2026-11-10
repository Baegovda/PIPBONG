#pragma once

#include "core/input/HotkeyBinding.h"
#include "model/UserInputInterruptMode.h"

#include <array>
#include <chrono>
#include <functional>
#include <mutex>
#include <memory>
#include <cstdint>
#include <string>
#include <unordered_map>

class ExecutionContext;

class UserInputInterruptMonitor {
public:
    using InterruptHandler = std::function<void(const std::string&, const std::shared_ptr<ExecutionContext>&)>;

    static UserInputInterruptMonitor& instance();

    void setHandler(InterruptHandler handler);
    void setHotkeyExemptionCheck(std::function<bool(int virtualKey)> check);

    void registerSession(const std::string& featureId,
                         UserInputInterruptMode mode,
                         const HotkeyBinding& featureHotkey,
                         const std::shared_ptr<ExecutionContext>& context,
                         bool keyboardInterrupt = true);
    void unregisterSession(const std::string& featureId);
    void unregisterAll();

    void notifyPhysicalInput(int virtualKey);

    void pollInputEdges();

private:
    UserInputInterruptMonitor() = default;

    struct SessionEntry {
        UserInputInterruptMode mode = UserInputInterruptMode::Stop;
        HotkeyBinding featureHotkey;
        std::weak_ptr<ExecutionContext> context;
        std::uint64_t registration = 0;
        bool keyboardInterrupt = true;
        std::chrono::steady_clock::time_point lastInterruptAt{};
    };

    void refreshPollTimer();
    void pollMouseButtonEdges();
    void pollKeyboardEdges();
    bool shouldSuppressUserInputInterruptOnAnySession(int virtualKey) const;

    mutable std::mutex m_mutex;
    InterruptHandler m_handler;
    std::function<bool(int virtualKey)> m_hotkeyExemptionCheck;
    std::unordered_map<std::string, SessionEntry> m_sessions;
    std::array<bool, 5> m_mouseButtonWasDown{};
    std::array<bool, 256> m_keyboardWasDown{};
    bool m_keyboardPollEnabled = false;
    std::uint64_t m_nextRegistration = 0, m_handlerRevision = 0;
};
