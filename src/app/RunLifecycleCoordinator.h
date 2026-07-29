#pragma once

#include "app/SessionRunPolicy.h"
#include "app/FeatureRunSession.h"

#include <QString>

#include <map>
#include <string>
#include <vector>

class Feature;

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

    /// User stop path (§8.21 R5.1b) — engine/lane teardown or `finishRunSession`.
    static void stopFeatureRun(class MainWindow& window, const std::string& featureId);

    /// Session teardown after run ends (§8.21 R5.1b).
    static void finishRunSession(class MainWindow& window,
                                 const std::string& featureId,
                                 bool success,
                                 const QString& message,
                                 bool deferUiUpdate);

    enum class ExistingSessionReconcileOutcome {
        NoExistingSession,
        AbortAlreadyActive,
        StaleRemoved,
    };

    /// Tear down stale session map entry before creating a new one in `startFeatureRun`.
    static ExistingSessionReconcileOutcome reconcileExistingSessionBeforeStart(
        class MainWindow& window,
        const std::string& featureId,
        bool holdHotkeyStart);

    /// Foreground/capture gate before `startFeatureRun` body (§8.21 R5.1a). `MainWindow` friend required.
    static RunStartForegroundOutcome evaluateRunStartForeground(class MainWindow& window,
                                                                class Feature* feature,
                                                                bool fromHotkey,
                                                                bool silentRestoreStart,
                                                                bool holdHotkeyStart);

    struct PreparedFeatureRunSession {
        FeatureRunSession session;
        bool useHoldKeyTapFastPath = false;
        int holdTapVirtualKey = 0;
    };

    /// Session shell + optional `WorkflowEngine` before `m_runSessions.emplace` (§8.21 R5.2).
    static PreparedFeatureRunSession prepareNewSession(class MainWindow& window,
                                                       Feature& feature,
                                                       const QString& profileId,
                                                       bool fromHotkey,
                                                       bool skipTargetActivationOnStart);

    enum class PreparedSessionLaunchOutcome {
        AwaitingRoiEdit,
        Launched,
    };

    /// After `m_runSessions.emplace`: capture lock, run UI prep, ROI gate, trigger/hold/workflow launch.
    static PreparedSessionLaunchOutcome activateAndLaunchPreparedSession(
        class MainWindow& window,
        Feature* feature,
        FeatureRunSession& activeSession,
        bool fromHotkey,
        bool deferTriggerRestoreStart,
        bool useHoldKeyTapFastPath,
        int holdTapVirtualKey,
        bool holdHotkeyStart);

    /// Full `MainWindow::startFeatureRun` orchestration (§8.21 R5.2); stutter scope stays on `MainWindow`.
    static void startFeatureRun(class MainWindow& window,
                                Feature* feature,
                                bool fromHotkey,
                                bool skipTargetActivationOnStart);
};
