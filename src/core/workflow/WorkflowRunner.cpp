#include "core/workflow/WorkflowRunner.h"

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "ui/OpenCvQtUtil.h"

#include <algorithm>
#include <chrono>

namespace {

WorkflowRunResult executeSingleBlock(const Workflow& workflow,
                                     int blockIndex,
                                     ExecutionContext& ctx,
                                     const WorkflowRunHooks* hooks);

int lastImageFindBlockIndex(const Workflow& workflow) {
    const auto& blocks = workflow.blocks();
    for (int i = static_cast<int>(blocks.size()) - 1; i >= 0; --i) {
        if (blocks[i] && blocks[i]->type() == BlockType::ImageFind) {
            return i;
        }
    }
    return -1;
}

int firstImageFindBlockIndex(const Workflow& workflow) {
    const auto& blocks = workflow.blocks();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (blocks[i] && blocks[i]->type() == BlockType::ImageFind) {
            return i;
        }
    }
    return -1;
}

int previousImageFindBlockIndex(const Workflow& workflow, int currentIndex) {
    for (int i = currentIndex - 1; i >= 0; --i) {
        const auto& blocks = workflow.blocks();
        if (i < static_cast<int>(blocks.size()) && blocks[i]
            && blocks[i]->type() == BlockType::ImageFind) {
            return i;
        }
    }
    const int last = lastImageFindBlockIndex(workflow);
    if (last >= 0 && last != currentIndex) {
        return last;
    }
    return -1;
}

int nextImageFindBlockIndex(const Workflow& workflow, int currentIndex) {
    const auto& blocks = workflow.blocks();
    for (int i = currentIndex + 1; i < static_cast<int>(blocks.size()); ++i) {
        if (blocks[i] && blocks[i]->type() == BlockType::ImageFind) {
            return i;
        }
    }
    const int first = firstImageFindBlockIndex(workflow);
    if (first >= 0 && first != currentIndex) {
        return first;
    }
    return -1;
}

const ImageFindBlock* imageFindBlockAt(const Workflow& workflow, int blockIndex) {
    const auto& blocks = workflow.blocks();
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size()) || !blocks[blockIndex]) {
        return nullptr;
    }
    if (blocks[blockIndex]->type() != BlockType::ImageFind) {
        return nullptr;
    }
    return static_cast<const ImageFindBlock*>(blocks[blockIndex].get());
}

struct ImageFindFailureResolution {
    bool handled = false;
    int continueAtIndex = -1;
    int jumpToBlockIndex = -1;
};

bool propagateNonDetectionStepFailure(const WorkflowRunResult& step, ExecutionContext& ctx) {
    return !step.success && !ctx.detectionFailedThisRun() && !ctx.shouldStop();
}

void notifyImageFindFailureHandling(int blockIndex,
                                    ExecutionContext& ctx,
                                    const WorkflowRunHooks* hooks) {
    if (!hooks || !hooks->onImageFindFailureHandling) {
        return;
    }
    hooks->onImageFindFailureHandling(blockIndex,
                                      ctx.imageFindReturnToPreviousCount(blockIndex),
                                      ctx.imageFindRetryAfterNextCount(blockIndex));
}

ImageFindFailureResolution resolveImageFindDetectionFailure(const Workflow& workflow,
                                                            int currentIndex,
                                                            ExecutionContext& ctx,
                                                            const WorkflowRunHooks* hooks,
                                                            const WorkflowRunResult& failedStep) {
    ImageFindFailureResolution resolution;
    const ImageFindBlock* imageFind = imageFindBlockAt(workflow, currentIndex);
    if (!imageFind || !ctx.detectionFailedThisRun()) {
        return resolution;
    }

    if (imageFind->retryAfterNextActionOnFailure && !ctx.imageFindDeferRetryUsed(currentIndex)) {
        ctx.markImageFindDeferRetryUsed(currentIndex);
        ctx.incrementImageFindRetryAfterNextCount(currentIndex);
        notifyImageFindFailureHandling(currentIndex, ctx, hooks);
        ctx.clearDetectionFailedFlag();

        const int blockCount = static_cast<int>(workflow.blocks().size());
        if (currentIndex + 1 < blockCount) {
            WorkflowRunResult nextStep =
                executeSingleBlock(workflow, currentIndex + 1, ctx, hooks);
            if (propagateNonDetectionStepFailure(nextStep, ctx)) {
                return resolution;
            }
        }

        ctx.log("화면에서 찾지 못함 → 다음 동작 후 재시도 (단계 #"
                + std::to_string(currentIndex + 1) + ")");
        resolution.handled = true;
        resolution.continueAtIndex = currentIndex;
        return resolution;
    }

    int previousImageFind = -1;
    if (imageFind->returnToPreviousImageFindOnFailure) {
        previousImageFind = previousImageFindBlockIndex(workflow, currentIndex);
        if (previousImageFind >= 0) {
            ctx.clearImageFindDeferRetryUsed(currentIndex);
            ctx.incrementImageFindReturnToPreviousCount(currentIndex);
            notifyImageFindFailureHandling(currentIndex, ctx, hooks);
            if (hooks && hooks->onImageFindReturnToPrevious) {
                hooks->onImageFindReturnToPrevious(currentIndex, previousImageFind);
            }
            ctx.clearDetectionFailedFlag();
            ctx.log("화면에서 찾지 못함 → 이전 검색 단계 #"
                    + std::to_string(previousImageFind + 1) + "으로 이동");
            resolution.handled = true;
            resolution.continueAtIndex = previousImageFind;
            return resolution;
        }
    }

    if (imageFind->retryAfterNextActionOnFailure && ctx.imageFindDeferRetryUsed(currentIndex)) {
        const int nextImageFind = nextImageFindBlockIndex(workflow, currentIndex);
        ctx.clearImageFindDeferRetryUsed(currentIndex);
        ctx.clearDetectionFailedFlag();
        if (nextImageFind >= 0) {
            ctx.incrementImageFindRetryAfterNextCount(currentIndex);
            notifyImageFindFailureHandling(currentIndex, ctx, hooks);
            ctx.log("화면에서 찾지 못함 → 다음 검색 단계 #"
                    + std::to_string(nextImageFind + 1) + "으로 이동");
            resolution.handled = true;
            resolution.continueAtIndex = nextImageFind;
            return resolution;
        }
        (void)failedStep;
    }

    return resolution;
}

bool applyImageFindFailureResolution(ImageFindFailureResolution& resolution,
                                     int rangeStartIndex,
                                     int rangeEndIndex,
                                     int& loopIndex,
                                     WorkflowRunResult& overall) {
    if (!resolution.handled || resolution.continueAtIndex < 0) {
        return false;
    }

    if (resolution.continueAtIndex < rangeStartIndex || resolution.continueAtIndex > rangeEndIndex) {
        overall.jumpToBlockIndex = resolution.continueAtIndex;
        overall.success = true;
        overall.message = "정상 완료";
        return true;
    }

    loopIndex = resolution.continueAtIndex;
    return true;
}

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
    overall.message = "정상 완료";

    const auto& blocks = workflow.blocks();
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size())) {
        overall.success = false;
        overall.message = "잘못된 단계 번호";
        return overall;
    }

    if (!ctx.waitWhilePaused()) {
        overall.success = false;
        overall.message = "사용자가 중지함";
        return overall;
    }
    if (ctx.shouldStop()) {
        overall.success = false;
        overall.message = "사용자가 중지함";
        return overall;
    }

    Block& block = *blocks[blockIndex];

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

    if (result.success && block.type() == BlockType::ImageFind) {
        ctx.clearImageFindDeferRetryUsed(blockIndex);
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
    overall.message = "정상 완료";

    for (int i = startIndex; i <= endIndex;) {
        WorkflowRunResult step = executeSingleBlock(workflow, i, ctx, hooks);
        if (!step.success) {
            ImageFindFailureResolution resolution =
                resolveImageFindDetectionFailure(workflow, i, ctx, hooks, step);
            if (applyImageFindFailureResolution(resolution, startIndex, endIndex, i, overall)) {
                if (overall.jumpToBlockIndex >= 0) {
                    return overall;
                }
                continue;
            }
            return step;
        }
        if (ctx.shouldStop()) {
            overall.success = false;
            overall.message = "사용자가 중지함";
            return overall;
        }
        ++i;
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
        result.message = "반복 구간이 너무 깊게 중첩됨";
        ctx.log(result.message);
        return result;
    }

    int iteration = 0;
    while (true) {
        if (!ctx.waitWhilePaused()) {
            ctx.leaveNestingScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자가 중지함";
            return result;
        }
        if (ctx.shouldStop()) {
            ctx.leaveNestingScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자가 중지함";
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

        if (runResult.jumpToBlockIndex >= 0) {
            ctx.leaveNestingScope();
            return runResult;
        }

        if (ctx.shouldStop()) {
            ctx.leaveNestingScope();
            WorkflowRunResult result;
            result.success = false;
            result.message = "사용자가 중지함";
            return result;
        }

        if (shouldExitLoopIteration(region.exitCondition, ctx, runResult.success)) {
            ctx.log("반복 구간 종료 · 단계 #"
                    + std::to_string(region.startIndex + 1) + "~#"
                    + std::to_string(region.endIndex + 1) + " · "
                    + loopExitConditionToString(region.exitCondition) + " · "
                    + std::to_string(iteration) + "회");
            ctx.leaveNestingScope();
            WorkflowRunResult result;
            result.success = true;
            result.message = "반복 구간 완료";
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

int WorkflowRunner::firstImageFindBlockIndex(const Workflow& workflow) {
    const auto& blocks = workflow.blocks();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (!blocks[i] || blocks[i]->type() != BlockType::ImageFind) {
            continue;
        }
        const auto* imageFind = static_cast<const ImageFindBlock*>(blocks[i].get());
        if (imageFind->hasTemplates()) {
            return i;
        }
    }
    return -1;
}

WorkflowRunResult WorkflowRunner::run(const Workflow& workflow,
                                      ExecutionContext& ctx,
                                      const WorkflowRunHooks* hooks) {
    const int monitorOnly = ctx.triggerMonitorBlockIndex();
    if (monitorOnly >= 0) {
        return executeSingleBlock(workflow, monitorOnly, ctx, hooks);
    }

    WorkflowRunResult overall;
    overall.success = true;
    overall.message = "정상 완료";

    const int blockCount = static_cast<int>(workflow.blocks().size());
    for (int i = 0; i < blockCount;) {
        if (!ctx.waitWhilePaused()) {
            overall.success = false;
            overall.message = "사용자가 중지함";
            break;
        }
        if (ctx.shouldStop()) {
            overall.success = false;
            overall.message = "사용자가 중지함";
            break;
        }

        if (const WorkflowLoopRegion* region = workflow.loopRegionStartingAt(i)) {
            WorkflowRunResult loopResult = runLoopRegion(workflow, *region, ctx, hooks);
            if (loopResult.jumpToBlockIndex >= 0) {
                i = loopResult.jumpToBlockIndex;
                continue;
            }
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
            ImageFindFailureResolution resolution =
                resolveImageFindDetectionFailure(workflow, i, ctx, hooks, step);
            if (applyImageFindFailureResolution(resolution, 0, blockCount - 1, i, overall)) {
                if (overall.jumpToBlockIndex >= 0) {
                    break;
                }
                continue;
            }
            overall.success = false;
            overall.message = step.message;
            break;
        }
        ++i;
    }

    return overall;
}
