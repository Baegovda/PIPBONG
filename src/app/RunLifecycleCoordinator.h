#pragma once

#include "app/SessionRunPolicy.h"
#include "app/FeatureRunSession.h"

#include <QString>

#include <map>
#include <string>
#include <vector>

class Feature;
class HoldKeyTapMultiplexer;
class QObject;
class WorkflowEngine;

/// Run session lifecycle policy helpers (AGENTS.md §8.21 R5.2).
/// Maps `FeatureRunSession` → `SessionRunPolicyInput` and continuation rules without HWND/hotkey I/O.
class RunLifecycleCoordinator {
public:
    static SessionRunPolicyInput policyInputFrom(const FeatureRunSession& session);

    /// Build policy inputs for all entries in the run session registry (optional `excludeFeatureId` skips one key).
    static std::vector<SessionRunPolicyInput> policyInputsFromSessions(
        const std::map<std::string, FeatureRunSession>& sessions,
        const std::string* excludeFeatureId = nullptr);

    static FeatureRunSession* sessionForId(std::map<std::string, FeatureRunSession>& sessions,
                                          const std::string& featureId);
    static const FeatureRunSession* sessionForId(const std::map<std::string, FeatureRunSession>& sessions,
                                                 const std::string& featureId);

    /// Resolve session from `WorkflowEngine` signal sender (active map + abandoned-engine fallback).
    static FeatureRunSession* sessionForEngine(class MainWindow& window, const QObject* sender);

    /// Hold retap: block new physical start while engine or mux lane is still active.
    static bool holdSessionBlocksNewPhysicalStart(const FeatureRunSession& session,
                                                  HoldKeyTapMultiplexer* mux);

    /// Tear down lane/engine/context and remove registry entry (§8.21 R5.2).
    static void tearDownAndEraseSessionEntry(class MainWindow& window, const std::string& featureId);

    /// Remove map entry and refresh run UI (e.g. ROI edit cancelled).
    static void eraseSessionEntryAndRefreshRunUi(class MainWindow& window, const std::string& featureId);

    /// `holdBindingStillActive` from `HotkeyManager::isHoldBindingStillActiveForRun` (Hold mode only).
    static bool shouldContinueSession(const FeatureRunSession& session, bool holdBindingStillActive);

    static bool shouldCoalesceRunUi(const std::map<std::string, FeatureRunSession>& sessions,
                                  bool deferUiUpdate = false);

    static bool shouldDeferAbandonedEnginePrune(const std::map<std::string, FeatureRunSession>& sessions,
                                                bool holdBurstUiActive);

    /// Hold-burst / coalesced hold-start UI window (§8.21 R5.2).
    static bool isHoldBurstUiActive(const class MainWindow& window);

    static bool shouldLogSessionDetailsInBurst(const class MainWindow& window);

    /// Hold hotkey burst: foreground prep + coalesced start/end UI and deferred burst side effects (§8.21 R5.2).
    static void prepareForegroundForHoldBurst(class MainWindow& window);
    static void scheduleCoalescedHoldStartUi(class MainWindow& window, const std::string& featureId);
    static void flushCoalescedHoldStartUi(class MainWindow& window);
    static void scheduleCoalescedHoldEndCleanup(class MainWindow& window);
    static void flushCoalescedHoldEndCleanup(class MainWindow& window);
    static void scheduleHoldBurstScopeDrain(class MainWindow& window);
    static void drainHoldBurstScope(class MainWindow& window);
    static void flushDeferredBurstSideEffects(class MainWindow& window);
    static void scheduleCoalescedHoldFeatureStart(class MainWindow& window, const std::string& featureId);
    static void flushCoalescedHoldFeatureStarts(class MainWindow& window);
    static void scheduleCoalescedHoldFeatureEndFinish(class MainWindow& window,
                                                      const std::string& featureId);
    static void flushCoalescedHoldFeatureEndFinishes(class MainWindow& window);

    /// User stop: shared flag teardown before engine/lane abandon (§8.21 R5.1b).
    static void applyUserStopRequestFlags(FeatureRunSession& session, bool suppressTriggerArmedPersist);

    enum class RunStartForegroundOutcome {
        Proceed,
        Abort,
        DeferTriggerRestore,
    };

    static void requestRunUiRefresh(class MainWindow& window, bool immediate);

    /// Coalesced run UI refresh body (§8.21 R5.3); GUI thread only — `MainWindow::applyRunUiState` asserts first.
    static void applyRunUiState(class MainWindow& window);

    /// User stop path (§8.21 R5.1b) — engine/lane teardown or `finishRunSession`.
    static void stopFeatureRun(class MainWindow& window, const std::string& featureId);

    /// Bulk stop for shutdown/update/profile teardown; does not persist trigger disarm (§8.21 R5.2).
    static void stopAllSessions(class MainWindow& window);

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

    /// Session shell + optional `WorkflowEngine` before registry emplace (§8.21 R5.2).
    static PreparedFeatureRunSession prepareNewSession(class MainWindow& window,
                                                       Feature& feature,
                                                       const QString& profileId,
                                                       bool fromHotkey,
                                                       bool skipTargetActivationOnStart);

    enum class PreparedSessionLaunchOutcome {
        AwaitingRoiEdit,
        Launched,
    };

    /// After registry emplace: capture lock, run UI prep, ROI gate, trigger/hold/workflow launch.
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
