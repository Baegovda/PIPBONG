#include "core/workflow/blocks/LoopBlock.h"
#include "core/workflow/LoopExitCondition.h"

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowRunner.h"
#include "storage/JsonSerializer.h"

bool LoopBlock::shouldExitAfterIteration(const ExecutionContext& ctx, bool bodySucceeded) const {
    return shouldExitLoopIteration(exitCondition, ctx, bodySucceeded);
}

std::string LoopBlock::summary() const {
    std::string condLabel;
    switch (exitCondition) {
    case LoopExitCondition::DetectionFailed:
        condLabel = "탐지 실패 시까지";
        break;
    case LoopExitCondition::DetectionSucceeded:
        condLabel = "탐지 성공 시까지";
        break;
    case LoopExitCondition::LastMatchSuccess:
        condLabel = "직전 매칭 성공 시까지";
        break;
    case LoopExitCondition::LastMatchFailed:
        condLabel = "직전 매칭 실패 시까지";
        break;
    }
    return condLabel + " · " + std::to_string(body.blocks().size()) + "블록";
}

BlockResult LoopBlock::execute(ExecutionContext& ctx) {
    if (!ctx.enterNestedScope()) {
        BlockResult result;
        result.success = false;
        result.message = "구간 반복 블록 중첩 깊이 초과";
        ctx.log(result.message);
        return result;
    }

    int iteration = 0;
    while (true) {
        if (!ctx.waitWhilePaused()) {
            ctx.leaveNestedScope();
            BlockResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }
        if (ctx.shouldStop()) {
            ctx.leaveNestedScope();
            BlockResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }

        ++iteration;
        ctx.clearDetectionFailedFlag();

        const int savedMissLimit = ctx.imageFindMaxMissAttempts();
        if (exitCondition == LoopExitCondition::DetectionFailed && detectionMissLimit > 0) {
            ctx.setImageFindMaxMissAttempts(detectionMissLimit);
        }

        WorkflowRunResult runResult = WorkflowRunner::run(body, ctx, nullptr);

        ctx.setImageFindMaxMissAttempts(savedMissLimit);

        if (ctx.shouldStop()) {
            ctx.leaveNestedScope();
            BlockResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            return result;
        }

        if (shouldExitAfterIteration(ctx, runResult.success)) {
            ctx.log("구간 반복 종료: " + loopExitConditionToString(exitCondition) + " ("
                    + std::to_string(iteration) + "회)");
            ctx.leaveNestedScope();
            BlockResult result;
            result.success = true;
            result.message = "구간 반복 완료";
            return result;
        }

        if (!runResult.success && exitCondition != LoopExitCondition::DetectionFailed) {
            ctx.leaveNestedScope();
            BlockResult result;
            result.success = false;
            result.message = runResult.message.empty() ? "구간 반복 본문 실패" : runResult.message;
            return result;
        }

        if (!runResult.success && exitCondition == LoopExitCondition::DetectionFailed
            && !ctx.detectionFailedThisRun()) {
            ctx.leaveNestedScope();
            BlockResult result;
            result.success = false;
            result.message = runResult.message.empty() ? "구간 반복 본문 실패" : runResult.message;
            return result;
        }
    }
}

nlohmann::json LoopBlock::toJson() const {
    nlohmann::json json{{"type", "Loop"},
                        {"exitCondition", loopExitConditionToString(exitCondition)},
                        {"body", JsonSerializer::workflowToJson(body)}};
    if (exitCondition == LoopExitCondition::DetectionFailed && detectionMissLimit != 1) {
        json["detectionMissLimit"] = detectionMissLimit;
    }
    return json;
}

std::unique_ptr<Block> LoopBlock::clone() const {
    auto copy = std::make_unique<LoopBlock>();
    copy->exitCondition = exitCondition;
    copy->detectionMissLimit = detectionMissLimit;
    copy->body.assignFrom(body);
    return copy;
}

std::unique_ptr<LoopBlock> LoopBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<LoopBlock>();
    block->exitCondition =
        loopExitConditionFromString(json.value("exitCondition", "DetectionFailed"));
    block->detectionMissLimit = std::max(1, json.value("detectionMissLimit", 1));
    if (json.contains("body")) {
        JsonSerializer::workflowFromJson(json["body"], block->body);
    }
    return block;
}
