#include "app/RunContextController.h"
#include "app/FeatureRunSession.h"
#include "model/Feature.h"
#include "core/capture/ScreenCapture.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <future>
#include <iostream>

namespace {
bool check(bool value, const char* label) {
    if (!value) std::cerr << "Run context check failed: " << label << '\n';
    return value;
}
bool until(const std::function<bool()>& predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 4000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    return predicate();
}
}

bool runRunContextChecks() {
#ifdef _WIN32
    ScreenCapture::TargetWindowInfo foreground;
    const HWND foregroundHwnd = GetForegroundWindow();
    if (foregroundHwnd && IsWindowVisible(foregroundHwnd)
        && ScreenCapture::queryWindowInfo(foregroundHwnd, foreground) && !foreground.title.empty()) {
        const auto positive = ScreenCapture::findVisibleWindowMatchingTitle(foreground.title, {});
        if (positive) {
            if (!check(ScreenCapture::findVisibleWindowMatchingTitle(foreground.title,
                L"C:\\__pipbong_nonexistent_process__\\no-such.exe") == nullptr,
                "a process mismatch never falls back to a title-only window")) return false;
            std::cout << "Target process constraint: read-only visible-window positive control and mismatched-process rejection OK (no activation)\n";
        } else std::cout << "Target process constraint: visible title changed before positive control; native lookup check skipped\n";
    } else std::cout << "Target process constraint: no readable visible window; native lookup check skipped\n";
#endif
    RunContextController::TargetBinding binding{L"Launcher", L"main.exe", L"launcher.exe"};
    if (!check(RunContextController::processPath(binding, FeatureCaptureTargetScope::MainOnly, L"Launcher") == L"main.exe"
        && RunContextController::processPath(binding, FeatureCaptureTargetScope::SubOnly, L"Main") == L"launcher.exe"
        && RunContextController::processPath(binding, FeatureCaptureTargetScope::Auto, L"App LAUNCHER") == L"launcher.exe"
        && RunContextController::processPath(binding, FeatureCaptureTargetScope::Auto, L"Main") == L"main.exe",
        "main/sub/auto target process routing")) return false;
    Feature feature("snapshot");
    feature.setRunMode(FeatureRunMode::Trigger);
    FeatureRunSession session;
    session.runningMode = FeatureRunMode::Trigger;
    session.triggerPhase = TriggerSessionPhase::Monitoring;
    session.sessionContext = std::make_shared<ExecutionContext>();
    RunContextController::apply(session, feature);
    session.workerGates->publish(true, false);
    auto context = session.sessionContext;
    auto gates = session.workerGates;
    std::atomic<bool> done{false};
    auto reader = std::async(std::launch::async, [context, &done] {
        while (!done.load()) {
            (void)context->scopedTargetPollAllowed();
            (void)context->triggerMonitorCaptureAllowed();
        }
        return !context->scopedTargetPollAllowed() && context->triggerMonitorCaptureAllowed();
    });
    for (int i = 0; i < 20000; ++i) gates->publish(i % 2 != 0, i % 3 != 0);
    gates->publish(false, true); done.store(true);
    if (!check(reader.get(), "concurrent atomic gate publication")) return false;
    session = FeatureRunSession{};
    if (!check(!context->scopedTargetPollAllowed() && context->triggerMonitorCaptureAllowed(),
               "callbacks retain gates after owning session is destroyed")) return false;

    context->setTargetWindowBindingForWorker(L"first", L"first.exe");
    done.store(false);
    std::promise<void> bindingReaderReady;
    auto bindingReaderStarted = bindingReaderReady.get_future();
    auto bindings = std::async(std::launch::async, [context, &done, &bindingReaderReady] {
        bool consistent = true;
        bool first = true;
        do {
            const auto value = context->targetWindowBinding();
            consistent = consistent && value.second == value.first + L".exe";
            if (first) { first = false; bindingReaderReady.set_value(); }
        } while (!done.load());
        return consistent;
    });
    if (bindingReaderStarted.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        done.store(true); bindings.get(); return check(false, "binding reader starts within deadline");
    }
    for (int i = 0; i < 20000; ++i) {
        const std::wstring title = i % 2 ? L"first" : L"second";
        context->setTargetWindowBindingForWorker(title, title + L".exe");
    }
    done.store(true);
    if (!check(bindings.get(), "title and process path remain a coherent snapshot under concurrent updates")) return false;

    auto workflow = std::make_shared<Workflow>();
    auto wait = std::make_unique<WaitBlock>(); wait->ms = 0; workflow->addBlock(std::move(wait));
    RunContextController::Preparation plan;
    plan.run = {workflow, context}; plan.targetTitle = L"owned-session-title";
    plan.targetProcessPath = L"owned.exe";
    plan.projectDirectory = "owned/project";
    plan.configureMonitor = true; plan.triggerBlockIndex = 3;
    plan.activateTarget = true; plan.requireTargetGate = true;
    plan.burstActivation = std::make_shared<std::atomic<bool>>(false);
    const auto burst = plan.burstActivation;
    std::atomic<int> activations{0};
    auto prepare = RunContextController::prepare(plan, [&] { ++activations; });
    const auto sourceWorkflow = workflow;
    workflow.reset(); plan = {};
    const auto originalGlobal = ScreenCapture::targetWindowTitle();
    auto prepared = std::async(std::launch::async, prepare).get();
    if (!check(prepared.workflow == sourceWorkflow && prepared.context == context
               && context->targetWindowTitle() == L"owned-session-title"
               && context->targetWindowBinding().second == L"owned.exe"
               && context->triggerMonitorBlockIndex() == 3 && activations == 0
               && ScreenCapture::targetWindowTitle() == originalGlobal,
               "preparer owns inputs, honors target gate and preserves global capture binding")) return false;
    gates->publish(true, true);
    std::vector<std::future<PreparedWorkflowRun>> starts;
    for (int i = 0; i < 8; ++i) {
        RunContextController::Preparation concurrent;
        concurrent.run = {sourceWorkflow, std::make_shared<ExecutionContext>()};
        concurrent.configureTarget = false;
        concurrent.activateTarget = true;
        concurrent.burstActivation = burst;
        starts.push_back(std::async(std::launch::async,
            RunContextController::prepare(std::move(concurrent), [&] { ++activations; })));
    }
    for (auto& start : starts) start.get();
    if (!check(activations == 1, "burst token permits activation once")) return false;
    context->requestStop(); context->setPaused(true);
    prepare();
    if (!check(context->shouldStop() && context->isPaused() && activations == 1,
               "worker preparation cannot erase pending stop or pause")) return false;

    RunContextController::Preparation action;
    action.run = {sourceWorkflow, context};
    context->setTriggerMonitorBlockIndex(-1);
    context->setImageFindPrimedBlockIndex(7);
    RunContextController::prepare(std::move(action), [] {})();
    if (!check(context->triggerMonitorBlockIndex() == -1 && context->consumeImageFindPrimedBlockIndex(7),
               "ordinary action preparation preserves primed trigger handoff")) return false;

    // Delay the real engine's preparer, then stop before it exposes its context.
    context->resetStop();
    std::promise<void> entered, release;
    auto releaseFuture = release.get_future().share();
    WorkflowEngine engine;
    engine.runPrepared([&entered, releaseFuture, prepare] {
        entered.set_value(); releaseFuture.wait(); return prepare();
    });
    if (entered.get_future().wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        release.set_value(); return check(false, "engine preparation starts within deadline");
    }
    engine.stop(); release.set_value();
    if (!check(until([&] { return !engine.isRunning(); }) && context->shouldStop(),
               "queued engine stop survives delayed owned preparation")) return false;
    engine.stopAndWaitBounded(1000);
    std::cout << "Run context: atomic poll/yield gates, coherent target/process binding, main/sub routing, detached lifetime, owned preparation, capture isolation, activation token and delayed-engine stop OK (recorded activation; no native input)\n";
    return true;
}
