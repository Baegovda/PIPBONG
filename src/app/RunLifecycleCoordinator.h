#pragma once

#include "app/SessionRunPolicy.h"

#include <map>
#include <string>
#include <vector>

struct FeatureRunSession;

/// Run session lifecycle policy helpers (AGENTS.md §8.21 R5.2).
/// Maps `FeatureRunSession` → `SessionRunPolicyInput` and continuation rules without HWND/hotkey I/O.
class RunLifecycleCoordinator {
public:
    static SessionRunPolicyInput policyInputFrom(const FeatureRunSession& session);

    /// Build policy inputs for all entries in `m_runSessions` (optional `excludeFeatureId` skips one key).
    static std::vector<SessionRunPolicyInput> policyInputsFromSessions(
        const std::map<std::string, FeatureRunSession>& sessions,
        const std::string* excludeFeatureId = nullptr);

    static FeatureRunSession* sessionForId(std::map<std::string, FeatureRunSession>& sessions,
                                          const std::string& featureId);
    static const FeatureRunSession* sessionForId(const std::map<std::string, FeatureRunSession>& sessions,
                                                 const std::string& featureId);

    /// `holdBindingStillActive` from `HotkeyManager::isHoldBindingStillActiveForRun` (Hold mode only).
    static bool shouldContinueSession(const FeatureRunSession& session, bool holdBindingStillActive);

    static bool shouldCoalesceRunUi(const std::map<std::string, FeatureRunSession>& sessions,
                                  bool deferUiUpdate = false);

    static bool shouldDeferAbandonedEnginePrune(const std::map<std::string, FeatureRunSession>& sessions,
                                                bool holdBurstUiActive);

    /// User stop: shared flag teardown before engine/lane abandon (§8.21 R5.1b).
    static void applyUserStopRequestFlags(FeatureRunSession& session, bool suppressTriggerArmedPersist);

    enum class RunStartForegroundOutcome {
        Proceed,
        Abort,
        DeferTriggerRestore,
    };

    static void requestRunUiRefresh(class MainWindow& window, bool immediate);

    /// Foreground/capture gate before `startFeatureRun` body (§8.21 R5.1a). `MainWindow` friend required.
    static RunStartForegroundOutcome evaluateRunStartForeground(class MainWindow& window,
                                                                class Feature* feature,
                                                                bool fromHotkey,
                                                                bool silentRestoreStart,
                                                                bool holdHotkeyStart);
};
