#pragma once

#include "app/SessionRunPolicy.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowEngine.h"
#include "model/FeatureRunMode.h"
#include "model/WorkflowExecutionHighlight.h"

#include <QElapsedTimer>
#include <QString>

#include <memory>
#include <string>
class WorkerRepeatController;
struct RunWorkerGates;

/// Per-feature run session (engine, worker context, trigger phase, UI run chrome).
///
/// Session-scoped capture (AGENTS.md §8.21 R3): `lockedCaptureTargetTitle` is fixed at
/// session start (main/sub/auto resolution) and reused for trigger monitor relaunches and
/// repeat/hold loop iterations when already set. Worker ImageFind polls must update capture
/// via `ExecutionContext::setTargetWindowTitleForWorker` / `refreshTargetWindowHandle` —
/// not global `ScreenCapture` writes from poll UI callbacks (`onBlockImageFindAttempt`).

struct TriggerPreemptedSession {
    std::string featureId;
    QString profileId;
    std::weak_ptr<ExecutionContext> context;
    bool pausedByTrigger = false;
    bool releasedMouseLock = false;
};

struct FeatureRunSession {
    std::string featureId;
    /// Profile that started this session; background sessions keep running after profile switch.
    QString profileId;
    std::string startedFeatureName;
    std::unique_ptr<WorkflowEngine> engine;
    bool usesHoldKeyTapFastPath = false;
    bool holdKeyTapLaneActive = false;
    quint64 holdKeyTapLaneToken = 0;
    FeatureRunMode runningMode = FeatureRunMode::RepeatCount;
    int repeatRemaining = 0;
    bool repeatSession = false;
    bool holdRunActive = false;
    bool holdHotkeyReleasedToTarget = false;
    bool holdReleaseBindingCaptured = false;
    int holdReleaseVirtualKey = 0;
    bool userStopRequested = false;
    /// GUI-owned first-ROI transaction; automatic launch/resume must wait.
    bool awaitingFirstTemplateRoi = false;
    /// When true, session teardown clears `triggerArmedFeatureIds` (explicit user stop only).
    bool disarmPersistedTrigger = false;
    quint64 holdRepeatGeneration = 0;
    int runningBlockIndex = -1;
    QString runningBlockSummary; // GUI-owned copy from the validated block-start event.
    int runningBlockSummaryIndex = -1;
    WorkflowExecutionHighlight runningBlockHighlight = WorkflowExecutionHighlight::None;
    std::shared_ptr<Workflow> sessionWorkflow;
    std::shared_ptr<ExecutionContext> sessionContext;
    std::shared_ptr<WorkerRepeatController> workerRepeat;
    std::shared_ptr<RunWorkerGates> workerGates;
    int sessionIteration = 0;
    bool hotkeyLaunchedSession = false;
    /// Hold-key-tap fast path: coalesced start UI runs once per session, not every runner relaunch.
    bool holdKeyTapStartUiDone = false;
    /// Skip the first workflow/trigger target-window activation (profile restore without focus steal).
    bool skipTargetActivationOnStart = false;
    bool pointerVisualFeedback = true;
    bool restoreMousePositionOnEnd = false;
    bool lockMouseToScreenCenterDuringRun = false;
    bool lockMouseToCurrentPositionDuringRun = false;
    int lockMouseDuringFirstLoopCount = 0;
    int unlockMouseOnBlockFailureCount = 1;
    bool earlyLoopMouseLockEngaged = false;
    bool earlyLoopMouseLockReleased = false;
    int earlyLoopMouseLockFailureCount = 0;
    bool hasMouseLockPosition = false;
    bool mouseLockAnchoredToTargetWindow = false;
    quint64 featureMouseLockToken=0,earlyMouseLockToken=0;
    quint64 mouseLockGeneration=0;
    std::weak_ptr<ExecutionContext> mouseLockCapturedContext;
    std::pair<std::wstring,std::wstring> mouseLockCapturedBinding;
    int mouseLockScreenX = 0;
    int mouseLockScreenY = 0;
    int mouseLockWindowOffsetX = 0;
    int mouseLockWindowOffsetY = 0;
    bool hasRunStartCursorPosition = false;
    int runStartCursorScreenX = 0;
    int runStartCursorScreenY = 0;
    QElapsedTimer loopTimer;
    bool hasLastLoopTiming = false;
    int lastLoopNumber = 0;
    qint64 lastLoopElapsedMs = 0;
    qint64 lastLoopAverageMs = 0;
    qint64 totalLoopElapsedMs = 0;
    int completedLoopCount = 0;
    bool lastLoopSuccess = true;
    int consecutiveDetectionFailLoops = 0;
    TriggerSessionPhase triggerPhase = TriggerSessionPhase::None;
    int triggerBlockIndex = -1;
    quint64 triggerCooldownGeneration = 0;
    quint64 triggerMonitorRestartGeneration = 0;
    /// Epoch ms when the current trigger cooldown ends; 0 when not in cooldown.
    qint64 triggerCooldownEndsAtEpochMs = 0;
    int triggerCooldownTotalMs = 0;
    std::vector<TriggerPreemptedSession> triggerPreemptedSessions;
    quint64 triggerPreemptToken = 0;
    bool triggerReleasedOwnMouseLockForPreempt = false;
    QElapsedTimer loopLogPublishTimer;
    int loopsSinceLastLogPublish = 0;
    /// Capture target title locked at session start (main vs sub); reused for trigger relaunches.
    /// Worker polls use ExecutionContext::setTargetWindowTitleForWorker — not global ScreenCapture per poll.
    std::wstring lockedCaptureTargetTitle;
    std::wstring lockedCaptureTargetProcessPath;
    bool waitingForScopedTargetForeground = false;
    bool triggerMonitorUiInitialized = false;
    /// Project workflow changed while the engine was running; refresh at the next safe boundary.
    bool deferredSessionWorkflowRefresh = false;
};
