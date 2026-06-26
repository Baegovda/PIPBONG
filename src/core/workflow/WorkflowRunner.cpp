#include "core/workflow/WorkflowRunner.h"

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowLoopRegion.h"

#include "ui/OpenCvQtUtil.h"

#include <algorithm>
#include <chrono>

namespace {

constexpr auto kImageFindMissUiMinInterval = std::chrono::milliseconds(100);

void emitBlockMatchResult(const WorkflowRunHooks& hooks,
                          int blockIndex,
                          ExecutionContext& ctx,
                          bool matched) {
    if (!hooks.onBlockMatchResult) {
        return;
    }

    bool hasClientPoint = false;
    int clientX = 0;
    int clientY = 0;
    if (matched && ctx.hasLastMatch()) {
        hasClientPoint = true;
        clientX = ctx.lastMatchPoint().x;
        clientY = ctx.lastMatchPoint().y;
    } else if (!matched && ctx.hasLastMatchAttemptPoint()) {
        hasClientPoint = true;
        clientX = ctx.lastMatchAttemptClientPoint().x;
        clientY = ctx.lastMatchAttemptClientPoint().y;
    }

    const double matchThreshold =
        matched ? ctx.lastMatchThreshold() : ctx.lastMatchAttemptThreshold();
    const double confidence = matched ? ctx.lastMatchConfidence() : ctx.lastMatchAttemptConfidence();

    QPixmap preview;
    if (matched && ctx.hasLastMatchPreview()) {
        preview = OpenCvQtUtil::matToPixmap(ctx.lastMatchPreview());
    }

    hooks.onBlockMatchResult(blockIndex,
                             matchThreshold,
                             confidence,
                             preview,
                             matched,
                             hasClientPoint,
                             clientX,
                             clientY);
}

WorkflowRunResult executeSingleBlock(const Workflow& workflow,
                                     int blockIndex,
                                     ExecutionContext& ctx,
                                     const WorkflowRunHooks* hooks) {
    WorkflowRunResult overall;
    overall.success = true;
    overall.message = "완료";

    const auto& blocks = workflow.blocks();
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size())) {
        overall.success = false;
        overall.message = "잘못된 블록 인덱스";
        return overall;
    }

    if (!ctx.waitWhilePaused()) {
        overall.success = false;
        overall.message = "사용자에 의해 중지됨";
        return overall;
    }
    if (ctx.shouldStop()) {
        overall.success = false;
        overall.message = "사용자에 의해 중지됨";
        return overall;
    }

    Block& block = *blocks[blockIndex];
    if (block.type() == BlockType::Comment) {
        block.execute(ctx);
        return overall;
    }

    if (hooks && hooks->onBlockStarted) {
        hooks->onBlockStarted(blockIndex, block.summary());
    }

    if (hooks && hooks->onBlockProgress) {
        ctx.setProgressCallback([hooks, blockIndex, &ctx](BlockProgressKind kind) {
            if (kind == BlockProgressKind::ImageFindMiss && ctx.hasLastMatchAttempt()) {
                static thread_local std::chrono::steady_clock::time_point lastMissUiEmit;
                const auto now = std::chrono::steady_clock::now();
                if (now - lastMissUiEmit < kImageFindMissUiMinInterval) {
                    return;
                }
                lastMissUiEmit = now;
                emitBlockMatchResult(*hooks, blockIndex, ctx, false);
            } else if (kind == BlockProgressKind::ImageFindSuccess && ctx.hasLastMatch()) {
                emitBlockMatchResult(*hooks, blockIndex, ctx, true);
            }
            hooks->onBlockProgress(blockIndex, kind);
        });
    } else {
        ctx.clearProgressCallback();
    }

    const auto blockStart = std::chrono::steady_clock::now();
    BlockResult result = block.execute(ctx);
    const auto blockEnd = std::chrono::steady_clock::now();
    const qint64 durationMs = std::max<qint64>(
        0, std::chrono::duration_cast<std::chrono::milliseconds>(blockEnd - blockStart).count());

    ctx.clearProgressCallback();

    if (hooks && hooks->onBlockFinished) {
        hooks->onBlockFinished(blockIndex,
                               result.success,
                               result.message,
                               durationMs,
                               result.imageFindMatchDurationMs);
    }

    overall.success = result.success;
    overall.message = result.message;
    overall.workflowJumpIndex = result.workflowJumpIndex;
    return overall;
}

WorkflowRunResult executeBlockRange(const Workflow& workflow,
                                    int startIndex,
                                    int endIndex,
                                    ExecutionContext& ctx,
                                    const WorkflowRunHooks* hooks) {
    WorkflowRunResult overall;
    overall.success = true;
    overall.message = "완료";

    for (int i = startIndex; i <= endIndex; ++i) {
        WorkflowRunResult step = executeSingleBlock(workflow, i, ctx, hooks);
        if (!step.success) {
            return step;
        }
        if (ctx.shouldStop()) {
            overall.success = false;
            overall.message = "사용자에 의해 중지됨";
            return overall;
        }
    }
    return overall;
}

WorkflowRunResult runLoopRegion(const Workflow& workflow,
                                const WorkflowLoopRegion& region,
                                ExecutionContext& ctx,
                                const WorkflowRunHooks* hooks) {
    if (!ctx.enterNestedScope()) {
        WorkflowRunResult result;
        result.success = false;
        result.message = "구간 반복 중첩 깊이 초과";
        ctx.log(result.message);
        return result;
    }

    int iteration = 0;
    while (true) {
        if (!ctx.waitWhilePaused()) {
            ctx.leaveNestedScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }
        if (ctx.shouldStop()) {
            ctx.leaveNestedScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }

        ++iteration;
        ctx.clearDetectionFailedFlag();

        const int savedMissLimit = ctx.imageFindMaxMissAttempts();
        if (region.exitCondition == LoopExitCondition::DetectionFailed && region.detectionMissLimit > 0) {
            ctx.setImageFindMaxMissAttempts(region.detectionMissLimit);
        }

        WorkflowRunResult runResult =
            executeBlockRange(workflow, region.startIndex, region.endIndex, ctx, hooks);

        ctx.setImageFindMaxMissAttempts(savedMissLimit);

        if (ctx.shouldStop()) {
            ctx.leaveNestedScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }

        if (shouldExitLoopIteration(region.exitCondition, ctx, runResult.success)) {
            ctx.log("구간 반복 종료: #" + std::to_string(region.startIndex + 1) + "~#"
                    + std::to_string(region.endIndex + 1) + " "
                    + loopExitConditionToString(region.exitCondition) + " (" + std::to_string(iteration)
                    + "회)");
            ctx.leaveNestedScope();
            WorkflowRunResult result;
            result.success = true;
            result.message = "구간 반복 완료";
            return result;
        }

        if (!runResult.success && region.exitCondition != LoopExitCondition::DetectionFailed) {
            ctx.leaveNestedScope();
            return runResult;
        }

        if (!runResult.success && region.exitCondition == LoopExitCondition::DetectionFailed
            && !ctx.detectionFailedThisRun()) {
            ctx.leaveNestedScope();
            return runResult;
        }
    }
}

} // namespace

WorkflowRunResult WorkflowRunner::run(const Workflow& workflow,
                                      ExecutionContext& ctx,
                                      const WorkflowRunHooks* hooks) {
    WorkflowRunResult overall;
    overall.success = true;
    overall.message = "완료";

    const int blockCount = static_cast<int>(workflow.blocks().size());
    for (int i = 0; i < blockCount;) {
        if (!ctx.waitWhilePaused()) {
            overall.success = false;
            overall.message = "사용자에 의해 중지됨";
            break;
        }
        if (ctx.shouldStop()) {
            overall.success = false;
            overall.message = "사용자에 의해 중지됨";
            break;
        }

        if (const WorkflowLoopRegion* region = workflow.loopRegionStartingAt(i)) {
            WorkflowRunResult loopResult = runLoopRegion(workflow, *region, ctx, hooks);
            if (!loopResult.success) {
                overall.success = false;
                overall.message = loopResult.message;
                break;
            }
            i = region->endIndex + 1;
            continue;
        }

        WorkflowRunResult step = executeSingleBlock(workflow, i, ctx, hooks);
        if (!step.success) {
            overall.success = false;
            overall.message = step.message;
            break;
        }
        if (step.workflowJumpIndex >= 0) {
            i = std::clamp(step.workflowJumpIndex, 0, blockCount - 1);
            continue;
        }
        ++i;
    }

    return overall;
}
