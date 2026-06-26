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
    /// 0-based index in the current workflow to run next; -1 = continue sequentially.
    int workflowJumpIndex = -1;
};

struct WorkflowRunHooks {
    std::function<void(int blockIndex, const std::string& summary)> onBlockStarted;
    std::function<void(int blockIndex, bool success, const std::string& message, qint64 durationMs,
                       int64_t imageFindMatchDurationMs)>
        onBlockFinished;
    std::function<void(int blockIndex, BlockProgressKind kind)> onBlockProgress;
    std::function<void(int blockIndex,
                       double matchThreshold,
                       double confidence,
                       const QPixmap& matchPreview,
                       bool matched,
                       bool hasClientPoint,
                       int clientX,
                       int clientY)>
        onBlockMatchResult;
};

class WorkflowRunner {
public:
    static WorkflowRunResult run(const Workflow& workflow,
                                 ExecutionContext& ctx,
                                 const WorkflowRunHooks* hooks = nullptr);
};
