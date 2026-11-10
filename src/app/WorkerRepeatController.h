#pragma once
#include "app/SessionRunPolicy.h"
#include "core/workflow/ExecutionContext.h"
#include <atomic>
#include <functional>
#include <memory>
class Feature;

/// Per-run immutable configuration plus atomic gates. Worker callbacks never
/// read Feature, MainWindow, the session registry or hotkey binding containers.
class WorkerRepeatController {
public:
    struct Options {
        int failureLimit = 0;
        int fixedDelayMs = 0;
        bool randomDelay = false;
        int minimumDelayMs = 0;
        int maximumDelayMs = 0;
    };
    struct Config {
        SessionRunPolicyInput policy;
        int initialFailures = 0;
        Options options;
    };
    WorkerRepeatController(Config config, std::weak_ptr<ExecutionContext> context,
                           std::function<bool()> holdReader);
    bool shouldContinue(bool success, bool detectionFailed);
    int delayMs() const;
    /// GUI publishes a complete value snapshot; workers never dereference Feature.
    static Options optionsFromFeature(const Feature& feature);
    void updateOptions(Options options);
    void setForegroundAllowed(bool allowed) { m_foregroundAllowed.store(allowed, std::memory_order_relaxed); }
    int consecutiveFailures() const { return m_failures.load(std::memory_order_relaxed); }
private:
    const Config m_config;
    std::shared_ptr<const Options> m_options;
    std::weak_ptr<ExecutionContext> m_context;
    const std::function<bool()> m_holdReader;
    std::atomic<bool> m_foregroundAllowed{true};
    std::atomic<int> m_failures{0};
};
