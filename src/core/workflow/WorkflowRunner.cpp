#include "core/workflow/WorkflowRunner.h"

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "ui/OpenCvQtUtil.h"

#include <algorithm>
#include <chrono>

namespace {

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
        ctx.setActiveBlockIndex(blockIndex);
        block.execute(ctx);
        ctx.setActiveBlockIndex(-1);
        return overall;
    }

    if (hooks && hooks->onBlockStarted) {
        hooks->onBlockStarted(blockIndex, block.summary());
    }

    if (hooks && hooks->onBlockProgress) {
        ctx.setProgressCallback([hooks, blockIndex, &ctx](BlockProgressKind kind) {
            if (kind == BlockProgressKind::ImageFindMiss && ctx.hasLastMatchAttempt()) {
                emitBlockMatchResult(*hooks, blockIndex, ctx, false);
            } else if (kind == BlockProgressKind::ImageFindSuccess && ctx.hasLastMatch()) {
                emitBlockMatchResult(*hooks, blockIndex, ctx, true);
            }
            if (hooks->onBlockImageFindAttempt
                && (kind == BlockProgressKind::ImageFindMiss
                    || kind == BlockProgressKind::ImageFindSuccess)) {
                double matchThreshold = 0.0;
                double detectedConfidence = 0.0;
                const bool matched = kind == BlockProgressKind::ImageFindSuccess;
                if (matched && ctx.hasLastMatch()) {
                    matchThreshold = ctx.lastMatchThreshold();
                    detectedConfidence = ctx.lastMatchConfidence();
                } else if (!matched && ctx.hasLastMatchAttempt()) {
                    matchThreshold = ctx.lastMatchAttemptThreshold();
                    detectedConfidence = ctx.lastMatchAttemptConfidence();
                }
                hooks->onBlockImageFindAttempt(blockIndex,
                                               ctx.imageFindPollAttempt(),
                                               matchThreshold,
                                               detectedConfidence,
                                               matched);
            }
            hooks->onBlockProgress(blockIndex, kind);
        });
    } else {
        ctx.clearProgressCallback();
    }

    if (hooks && hooks->onPointerFeedbackAtClientPoint) {
        ctx.setPointerFeedbackCallback([hooks](int clientX, int clientY) {
            hooks->onPointerFeedbackAtClientPoint(clientX, clientY);
        });
    } else {
        ctx.clearPointerFeedbackCallback();
    }

    const auto blockStart = std::chrono::steady_clock::now();
    ctx.setActiveBlockIndex(blockIndex);
    BlockResult result = block.execute(ctx);
    ctx.setActiveBlockIndex(-1);
    const auto blockEnd = std::chrono::steady_clock::now();
    const qint64 durationMs = std::max<qint64>(
        0, std::chrono::duration_cast<std::chrono::milliseconds>(blockEnd - blockStart).count());

    ctx.clearProgressCallback();
    ctx.clearPointerFeedbackCallback();

    if (block.type() == BlockType::ImageFind && hooks) {
        if (result.success && ctx.hasLastMatch()) {
            emitBlockMatchResult(*hooks, blockIndex, ctx, true);
        } else if (!result.success && ctx.hasLastMatchAttempt()) {
            emitBlockMatchResult(*hooks, blockIndex, ctx, false);
        }
    }

    if (hooks && hooks->onBlockFinished) {
        hooks->onBlockFinished(blockIndex,
                               result.success,
                               result.message,
                               durationMs,
                               result.imageFindMatchDurationMs,
                               result.imageFindPollAttempts);
    }

    overall.success = result.success;
    overall.message = result.message;
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
    if (!ctx.enterNestingScope()) {
        WorkflowRunResult result;
        result.success = false;
        result.message = "구간 반복 중첩 깊이 초과";
        ctx.log(result.message);
        return result;
    }

    int iteration = 0;
    while (true) {
        if (!ctx.waitWhilePaused()) {
            ctx.leaveNestingScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }
        if (ctx.shouldStop()) {
            ctx.leaveNestingScope();
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
            ctx.leaveNestingScope();
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
            ctx.leaveNestingScope();
            WorkflowRunResult result;
            result.success = true;
            result.message = "구간 반복 완료";
            return result;
        }

        if (!runResult.success && region.exitCondition != LoopExitCondition::DetectionFailed) {
            ctx.leaveNestingScope();
            return runResult;
        }

        if (!runResult.success && region.exitCondition == LoopExitCondition::DetectionFailed
            && !ctx.detectionFailedThisRun()) {
            ctx.leaveNestingScope();
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
        ++i;
    }

    return overall;
}
