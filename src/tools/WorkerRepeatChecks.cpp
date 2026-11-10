#include "app/WorkerRepeatController.h"
#include "core/input/HoldStateSnapshot.h"
#include "core/workflow/HoldKeyTapMultiplexer.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <future>
#include <iostream>

namespace {
bool expect(bool value, const char* label) {
    if (!value) std::cerr << "Worker repeat check failed: " << label << '\n';
    return value;
}
bool waitFor(const std::function<bool()>& predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 4000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    return predicate();
}
}

bool runWorkerRepeatChecks() {
    auto context = std::make_shared<ExecutionContext>();
    WorkerRepeatController::Config config;
    config.policy.repeatSession = true;
    config.policy.runningMode = FeatureRunMode::RepeatCount;
    context->setWorkerRepeatRemaining(4);
    WorkerRepeatController count(config, context, {});
    if (!expect(count.shouldContinue(true, false) && count.shouldContinue(true, false)
                && count.shouldContinue(true, false) && !count.shouldContinue(true, false)
                && context->workerRepeatRemaining() == 0, "finite repeat count stops exactly at zero")) return false;
    context->setWorkerRepeatRemaining(4);
    if (!expect(!count.shouldContinue(false, false) && context->workerRepeatRemaining() == 4,
                "failed finite iteration does not consume a success count")) return false;
    config.policy.runningMode = FeatureRunMode::RepeatInfinite;
    config.options.failureLimit = 3;
    config.options.randomDelay = true;
    config.options.minimumDelayMs = 9;
    config.options.maximumDelayMs = 4;
    WorkerRepeatController infinite(config, context, {});
    if (!expect(infinite.shouldContinue(false, true) && infinite.consecutiveFailures() == 1
                && infinite.shouldContinue(true, false) && infinite.consecutiveFailures() == 0
                && infinite.shouldContinue(false, true) && infinite.shouldContinue(false, true)
                && !infinite.shouldContinue(false, true) && infinite.consecutiveFailures() == 3,
                "consecutive detection failure limit and reset")) return false;
    for (int i = 0; i < 100; ++i) {
        const int delay = infinite.delayMs();
        if (delay < 4 || delay > 9) std::cerr << "Unexpected captured delay: " << delay << " (expected 4..9)\n";
        if (!expect(delay >= 4 && delay <= 9, "random delay uses captured normalized bounds")) return false;
    }
    config.options.minimumDelayMs = config.options.maximumDelayMs = 0;
    config.options.fixedDelayMs = 17;
    WorkerRepeatController fallback(config, context, {});
    config.options.fixedDelayMs = 999;
    if (!expect(fallback.delayMs() == 17, "immutable configuration and unset-random fallback")) return false;
    WorkerRepeatController::Options updated;
    updated.fixedDelayMs = 23;
    fallback.updateOptions(updated);
    if (!expect(fallback.delayMs() == 23, "GUI-published option change replaces complete worker snapshot")) return false;

    SharedHoldLatch latch(true);
    const auto registered = std::make_shared<std::atomic<bool>>(true);
    HoldStateSnapshot snapshot{{}, false, latch.state(), registered};
    if (!expect(snapshot.active([] { return false; }), "latched hold survives synthetic physical-up gap")) return false;
    latch = false;
    if (!expect(snapshot.active([] { return true; }) && !snapshot.active([] { return false; }), "physical hold and release")) return false;
    registered->store(false);
    if (!expect(!snapshot.active([] { return true; }), "unregistered binding cannot remain active")) return false;
    registered->store(true);
    latch = true;
    config.policy.runningMode = FeatureRunMode::Hold;
    config.policy.holdRunActive = true;
    config.options.failureLimit = 0;
    WorkerRepeatController hold(config, context, [snapshot] { return snapshot.active([] { return false; }); });
    if (!expect(hold.shouldContinue(true, false), "hold policy uses lifetime-safe latch snapshot")) return false;
    hold.setForegroundAllowed(false);
    if (!expect(!hold.shouldContinue(true, false), "atomic GUI gate stops worker continuation")) return false;
    hold.setForegroundAllowed(true);
    context->requestStop();
    if (!expect(!hold.shouldContinue(true, false), "stop wins over active hold and foreground")) return false;
    context->resetStop();
    auto reader = std::async(std::launch::async, [snapshot] {
        for (int i = 0; i < 10000; ++i) (void)snapshot.active([] { return false; });
    });
    for (int i = 0; i < 10000; ++i) latch = (i % 2) != 0;
    reader.get();

    // Real multiplexer, recording emitter: no OS key/mouse input or native hooks.
    std::atomic<int> emitted{0};
    HoldKeyTapMultiplexer mux(nullptr, [&](int, ExecutionContext&) { ++emitted; });
    latch = true;
    mux.startLane("snapshot", 'A', 1, [snapshot] { return snapshot.active([] { return false; }); }, context);
    if (!expect(waitFor([&] { return emitted.load() >= 3; }), "real hold lane starts")) return false;
    latch = false;
    if (!expect(waitFor([&] { return !mux.isLaneActive("snapshot"); }), "snapshot release ends real hold lane")) return false;
    mux.stopAll();

    // Actual worker engine consumes GUI-published gate; no Feature/window/map read.
    auto engineContext = std::make_shared<ExecutionContext>();
    engineContext->setSuppressRepeatUi(true);
    config.policy.runningMode = FeatureRunMode::RepeatInfinite;
    config.options.randomDelay = false;
    config.options.fixedDelayMs = 0;
    auto runtime = std::make_shared<WorkerRepeatController>(config, engineContext, std::function<bool()>());
    std::atomic<int> iterations{0};
    ExecutionContext::WorkerFastRepeatCallbacks callbacks;
    callbacks.delayMs = [runtime] { return runtime->delayMs(); };
    callbacks.shouldContinue = [runtime](bool success, bool detection) { return runtime->shouldContinue(success, detection); };
    callbacks.onIterationComplete = [&](bool, std::int64_t, const std::string&) { ++iterations; };
    engineContext->setWorkerFastRepeatCallbacks(std::move(callbacks));
    auto workflow = std::make_shared<Workflow>();
    auto block = std::make_unique<WaitBlock>(); block->ms = 1;
    workflow->addBlock(std::move(block));
    WorkflowEngine engine;
    bool finished = false;
    QObject::connect(&engine, &WorkflowEngine::finished, &engine, [&](bool, const QString&) { finished = true; });
    QTimer gate;
    QObject::connect(&gate, &QTimer::timeout, [&] { if (iterations.load() >= 5) runtime->setForegroundAllowed(false); });
    gate.start(1);
    engine.run(workflow, engineContext);
    const bool stopped = waitFor([&] { return finished; });
    gate.stop();
    if (!expect(stopped && iterations.load() >= 5 && engine.stopAndWaitBounded(1000), "GUI gate ends real worker repeat")) return false;
    std::cout << "Worker repeat: immutable policy, failure/count/delay, atomic gate/latch, real engine and recorded hold-lane release OK\n";
    return true;
}
