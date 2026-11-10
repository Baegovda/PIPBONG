#pragma once
#include "core/workflow/WorkflowEngine.h"
#include "model/FeatureCaptureTargetScope.h"
#include <atomic>

class Feature;
struct FeatureRunSession;

/// Shared by the GUI publisher and worker-owned callbacks. No GUI pointers.
struct RunWorkerGates {
    std::atomic<unsigned> flags{0};
    void publish(bool targetAllowed, bool captureAllowed) {
        flags.store((targetAllowed ? 1u : 0u) | (captureAllowed ? 2u : 0u), std::memory_order_release);
    }
    bool targetAllowed() const { return (flags.load(std::memory_order_acquire) & 1u) != 0; }
    bool captureAllowed() const { return (flags.load(std::memory_order_acquire) & 2u) != 0; }
};

class RunContextController {
public:
    /// GUI/idle only: configure context values and install callbacks which own gates.
    static void apply(FeatureRunSession& session, const Feature& feature);
    struct TargetBinding {
        std::wstring subTitle;
        std::wstring mainProcessPath;
        std::wstring subProcessPath;
        std::wstring mainTitle;
    };
    static std::wstring processPath(const TargetBinding& binding, FeatureCaptureTargetScope scope,
                                    const std::wstring& targetTitle);
    struct Preparation {
        PreparedWorkflowRun run;
        std::wstring targetTitle;
        std::wstring targetProcessPath;
        std::string projectDirectory;
        bool configureTarget = true;
        bool activateTarget = false;
        bool requireTargetGate = false;
        bool configureMonitor = false;
        int triggerBlockIndex = -1;
        std::shared_ptr<std::atomic<bool>> burstActivation;
    };
    /// Owns all inputs by value; caller resets stop before queueing, never in worker.
    static WorkflowRunPreparer prepare(Preparation preparation,
                                      std::function<void()> activate = {});
};
