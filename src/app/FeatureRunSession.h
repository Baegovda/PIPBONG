#pragma once

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowEngine.h"
#include "model/FeatureRunMode.h"
#include "ui/BlockListWidget.h"

#include <QElapsedTimer>
#include <memory>
#include <string>

enum class TriggerSessionPhase {
    None,
    Monitoring,
    RunningAction,
    Cooldown
};

struct TriggerPreemptedSession {
    std::string featureId;
    bool pausedByTrigger = false;
    bool releasedMouseCenterLock = false;
};

struct FeatureRunSession {
    std::string featureId;
    std::unique_ptr<WorkflowEngine> engine;
    FeatureRunMode runningMode = FeatureRunMode::RepeatCount;
    int repeatRemaining = 0;
    bool repeatSession = false;
    bool holdRunActive = false;
    bool userStopRequested = false;
    quint64 holdRepeatGeneration = 0;
    int runningBlockIndex = -1;
    BlockListWidget::ExecutionHighlight runningBlockHighlight = BlockListWidget::ExecutionHighlight::None;
    std::shared_ptr<Workflow> sessionWorkflow;
    std::shared_ptr<ExecutionContext> sessionContext;
    int sessionIteration = 0;
    bool hotkeyLaunchedSession = false;
    bool pointerVisualFeedback = true;
    bool restoreMousePositionOnEnd = false;
    bool lockMouseToScreenCenterDuringRun = false;
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
    std::vector<TriggerPreemptedSession> triggerPreemptedSessions;
    bool triggerPreemptSavedCursor = false;
    int triggerPreemptCursorScreenX = 0;
    int triggerPreemptCursorScreenY = 0;
    bool triggerReleasedOwnMouseCenterLockForPreempt = false;
};
