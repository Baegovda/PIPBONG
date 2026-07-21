#include "core/workflow/LoopExitCondition.h"

#include "core/workflow/ExecutionContext.h"

std::string loopExitConditionToString(LoopExitCondition condition) {
    switch (condition) {
    case LoopExitCondition::DetectionSucceeded:
        return "DetectionSucceeded";
    case LoopExitCondition::LastMatchSuccess:
        return "LastMatchSuccess";
    case LoopExitCondition::LastMatchFailed:
        return "LastMatchFailed";
    case LoopExitCondition::DetectionFailed:
    default:
        return "DetectionFailed";
    }
}

LoopExitCondition loopExitConditionFromString(const std::string& value) {
    if (value == "DetectionSucceeded") {
        return LoopExitCondition::DetectionSucceeded;
    }
    if (value == "LastMatchSuccess") {
        return LoopExitCondition::LastMatchSuccess;
    }
    if (value == "LastMatchFailed") {
        return LoopExitCondition::LastMatchFailed;
    }
    return LoopExitCondition::DetectionFailed;
}

bool shouldExitLoopIteration(LoopExitCondition condition, const ExecutionContext& ctx, bool bodySucceeded) {
    switch (condition) {
    case LoopExitCondition::DetectionFailed:
        return !bodySucceeded && ctx.detectionFailedThisRun();
    case LoopExitCondition::DetectionSucceeded:
        return bodySucceeded && ctx.hasLastMatch();
    case LoopExitCondition::LastMatchSuccess:
        return ctx.hasLastMatch();
    case LoopExitCondition::LastMatchFailed:
        return !ctx.hasLastMatch() || ctx.detectionFailedThisRun();
    }
    return false;
}

std::string loopExitConditionDisplayLabel(LoopExitCondition condition) {
    switch (condition) {
    case LoopExitCondition::DetectionFailed:
        return "탐지 실패 시까지";
    case LoopExitCondition::DetectionSucceeded:
        return "탐지 성공 시까지";
    case LoopExitCondition::LastMatchSuccess:
        return "직전 매칭 성공 시까지";
    case LoopExitCondition::LastMatchFailed:
        return "직전 매칭 실패 시까지";
    }
    return "탐지 실패 시까지";
}

std::string loopExitConditionShortLabel(LoopExitCondition condition) {
    switch (condition) {
    case LoopExitCondition::DetectionFailed:
        return "실패까지";
    case LoopExitCondition::DetectionSucceeded:
        return "성공까지";
    case LoopExitCondition::LastMatchSuccess:
        return "직전성공";
    case LoopExitCondition::LastMatchFailed:
        return "직전실패";
    }
    return "실패까지";
}
