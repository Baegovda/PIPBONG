// PIPBONGWorkflowDryRunSim — headless WorkflowRunner smoke (AGENTS.md §8.21 R6.2).

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowRunner.h"
#include "core/workflow/blocks/WaitBlock.h"

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

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!runWaitOnlyWorkflow(0)) {
        return 1;
    }
    std::cout << "PIPBONGWorkflowDryRunSim: wait-only WorkflowRunner OK (R6.2)\n";
    return 0;
}
