#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"

#include <QPixmap>
#include <QString>
#include <functional>

class ExecutionContext;

struct WorkflowRunResult {
    bool success = true;
    std::string message;
    /// When >= 0, caller continues the parent workflow at this block index.
    int jumpToBlockIndex = -1;
};

struct WorkflowRunHooks {
    std::function<void(int blockIndex, const std::string& summary)> onBlockStarted;
    std::function<void(int blockIndex, bool success, const std::string& message, qint64 durationMs,
                       int64_t imageFindMatchDurationMs, int imageFindPollAttempts)>
        onBlockFinished;
    std::function<void(int blockIndex, BlockProgressKind kind)> onBlockProgress;
    std::function<void(int blockIndex,
                       int attemptCount,
                       double matchThreshold,
                       double detectedConfidence,
                       bool matched)>
        onBlockImageFindAttempt;
    std::function<void(int blockIndex,
                       double matchThreshold,
                       double confidence,
                       const QPixmap& matchPreview,
                       bool matched,
                       bool hasClientPoint,
                       int clientX,
                       int clientY)>
        onBlockMatchResult;
    std::function<void(int blockIndex, int returnToPreviousCount, int retryAfterNextCount)>
        onImageFindFailureHandling;
    std::function<void(int clientX, int clientY)> onPointerFeedbackAtClientPoint;
};

class WorkflowRunner {
public:
    static WorkflowRunResult run(const Workflow& workflow,
                                 ExecutionContext& ctx,
                                 const WorkflowRunHooks* hooks = nullptr);
};
