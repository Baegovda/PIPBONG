#pragma once

#include <string>

enum class LoopExitCondition {
    DetectionFailed,
    DetectionSucceeded,
    LastMatchSuccess,
    LastMatchFailed
};

std::string loopExitConditionToString(LoopExitCondition condition);
LoopExitCondition loopExitConditionFromString(const std::string& value);

class ExecutionContext;
bool shouldExitLoopIteration(LoopExitCondition condition, const ExecutionContext& ctx, bool bodySucceeded);
std::string loopExitConditionDisplayLabel(LoopExitCondition condition);
