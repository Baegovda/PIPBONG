#include "app/WorkerRepeatController.h"
#include "model/Feature.h"
#include <algorithm>
#include <random>
#include <utility>

WorkerRepeatController::WorkerRepeatController(Config config, std::weak_ptr<ExecutionContext> context,
                                               std::function<bool()> holdReader)
    : m_config(std::move(config)), m_options(std::make_shared<const Options>(m_config.options)),
      m_context(std::move(context)), m_holdReader(std::move(holdReader)),
      m_failures(m_config.initialFailures) {}

bool WorkerRepeatController::shouldContinue(bool success, bool detectionFailed) {
    const auto context = m_context.lock();
    if (!context || context->shouldStop()) return false;
    const auto mode = m_config.policy.runningMode;
    const auto options = std::atomic_load(&m_options);
    if (options->failureLimit > 0 && (mode == FeatureRunMode::RepeatInfinite || mode == FeatureRunMode::Hold)) {
        if (success) m_failures.store(0, std::memory_order_relaxed);
        else if (!detectionFailed) return false;
        else if (m_failures.fetch_add(1, std::memory_order_relaxed) + 1 >= options->failureLimit) return false;
    }
    if (mode == FeatureRunMode::RepeatCount) {
        if (!success) return false;
        if (m_config.policy.repeatSession) context->decrementWorkerRepeatRemainingAfterSuccess();
    }
    if (!m_foregroundAllowed.load(std::memory_order_relaxed)) return false;
    auto policy = m_config.policy;
    policy.repeatRemaining = context->workerRepeatRemaining();
    const bool holdActive = mode != FeatureRunMode::Hold || (m_holdReader && m_holdReader());
    return SessionRunPolicy::shouldContinueSession(policy, holdActive);
}

int WorkerRepeatController::delayMs() const {
    const auto options = std::atomic_load(&m_options);
    const int low = std::min(options->minimumDelayMs, options->maximumDelayMs);
    const int high = std::max(options->minimumDelayMs, options->maximumDelayMs);
    if (!options->randomDelay || (low <= 0 && high <= 0)) return std::max(0, options->fixedDelayMs);
    thread_local std::mt19937 random(std::random_device{}());
    std::uniform_int_distribution<int> distribution(low, high);
    const int sampled = distribution(random);
    return sampled < 0 ? 0 : sampled;
}

WorkerRepeatController::Options WorkerRepeatController::optionsFromFeature(const Feature& feature) {
    return {feature.infiniteExitAfterConsecutiveMisses(), feature.loopIntervalMs(), feature.loopIntervalRandomRange(),
            feature.loopIntervalMinMs(), feature.loopIntervalMaxMs()};
}

void WorkerRepeatController::updateOptions(Options options) {
    const auto before = std::atomic_load(&m_options);
    if (before->failureLimit == options.failureLimit && before->fixedDelayMs == options.fixedDelayMs
        && before->randomDelay == options.randomDelay && before->minimumDelayMs == options.minimumDelayMs
        && before->maximumDelayMs == options.maximumDelayMs) return;
    std::atomic_store(&m_options, std::make_shared<const Options>(options));
}
