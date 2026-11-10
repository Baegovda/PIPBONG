#pragma once
#include "core/input/HotkeyBinding.h"
#include "model/UserInputInterruptMode.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
class ExecutionContext;
class Feature;
struct FeatureRunSession;

// GUI-thread session policy. Native polling/queued delivery stays in the monitor.
// No widgets, MainWindow dependency or synthesized input.
class InputInterruptController {
public:
    enum class Outcome { Ignored, Stopped, Paused, Resumed };
    struct Registration {
        std::string featureId;
        UserInputInterruptMode mode;
        HotkeyBinding hotkey;
        std::shared_ptr<ExecutionContext> context;
        bool keyboard = true;
    };
    struct Ports {
        std::function<void(const Registration&)> subscribe;
        std::function<void(const std::string&)> unsubscribe;
    };
    struct Host {
        std::function<Feature*(FeatureRunSession&)> feature;
        std::function<void(const FeatureRunSession&)> disarm;
        std::function<void(const FeatureRunSession&, const QString&, Outcome)> changed;
    };
    InputInterruptController();
    explicit InputInterruptController(Ports ports) : m_ports(std::move(ports)) {}
    void setHost(Host host) { m_host = std::move(host); }
    void sync(FeatureRunSession& session);
    Outcome interrupt(FeatureRunSession& session, const std::shared_ptr<ExecutionContext>& expected);
private:
    Ports m_ports;
    Host m_host;
};
