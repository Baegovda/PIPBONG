// PIPBONGWorkflowDryRunSim — headless WorkflowRunner smoke (AGENTS.md §8.21 R6.2).

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/LoopExitCondition.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "core/workflow/WorkflowRunner.h"
#include "core/workflow/blocks/WaitBlock.h"

#include <opencv2/core.hpp>

#include <iostream>
#include <memory>

namespace {

bool runWaitOnlyWorkflow(int waitMs) {
    Workflow workflow;
    auto block = std::make_unique<WaitBlock>();
    block->ms = waitMs;
    block->randomRange = false;
    workflow.addBlock(std::move(block));

    ExecutionContext ctx;
    const WorkflowRunResult result = WorkflowRunner::run(workflow, ctx, nullptr);
    if (!result.success) {
        std::cerr << "wait workflow failed: " << result.message << '\n';
        return false;
    }
    return true;
}

bool runLoopRegionExitsAfterOneIteration(LoopExitCondition exitCondition, bool primeLastMatch) {
    Workflow workflow;
    auto block = std::make_unique<WaitBlock>();
    block->ms = 0;
    block->randomRange = false;
    workflow.addBlock(std::move(block));

    WorkflowLoopRegion region;
    region.startIndex = 0;
    region.endIndex = 0;
    region.exitCondition = exitCondition;
    workflow.setLoopRegions({region});

    int blockStarts = 0;
    WorkflowRunHooks hooks;
    hooks.onBlockStarted = [&](int, const std::string&) { ++blockStarts; };

    ExecutionContext ctx;
    if (primeLastMatch) {
        ctx.setLastMatch(cv::Point(10, 10), 0.99, cv::Mat(), 0.85);
    }

    const WorkflowRunResult result = WorkflowRunner::run(workflow, ctx, &hooks);
    if (!result.success) {
        std::cerr << "loop region failed: " << result.message << '\n';
        return false;
    }
    if (blockStarts != 1) {
        std::cerr << "loop region expected 1 block start, got " << blockStarts << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!runWaitOnlyWorkflow(0)) {
        return 1;
    }
    if (!runLoopRegionExitsAfterOneIteration(LoopExitCondition::LastMatchFailed, false)) {
        return 1;
    }
    if (!runLoopRegionExitsAfterOneIteration(LoopExitCondition::LastMatchSuccess, true)) {
        return 1;
    }
    std::cout << "PIPBONGWorkflowDryRunSim: wait + loop-region WorkflowRunner OK (R6.2)\n";
    return 0;
}
