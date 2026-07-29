// PIPBONGWorkflowDryRunSim — headless WorkflowRunner smoke (AGENTS.md §8.21 R6.2).

#include "core/capture/ScreenCapture.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/LoopExitCondition.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "core/workflow/WorkflowRunner.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

class DryRunCaptureScope {
public:
    explicit DryRunCaptureScope(const cv::Mat& haystack) {
        ScreenCapture::setRunWithoutTargetWindowOverrideForDryRun(true);
        ScreenCapture::setInjectedImageFindHaystackForDryRun(haystack);
    }
    ~DryRunCaptureScope() {
        ScreenCapture::clearInjectedImageFindHaystackForDryRun();
        ScreenCapture::setRunWithoutTargetWindowOverrideForDryRun(false);
    }
};

std::filesystem::path prepareTemplateProject(cv::Mat& haystack,
                                             cv::Mat& matchPatch,
                                             cv::Mat& nomatchPatch) {
    haystack = cv::Mat(160, 160, CV_8UC3, cv::Scalar(80, 80, 80));
    const cv::Rect roi(60, 60, 40, 40);
    matchPatch = cv::Mat(40, 40, CV_8UC3, cv::Scalar(200, 200, 200));
    matchPatch.copyTo(haystack(roi));
    nomatchPatch = cv::Mat(40, 40, CV_8UC3);
    cv::randu(nomatchPatch, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "pipbong_workflow_dry_run";
    const std::filesystem::path templates = root / "templates";
    std::filesystem::create_directories(templates);
    cv::imwrite((templates / "match.png").string(), matchPatch);
    cv::imwrite((templates / "nomatch.png").string(), nomatchPatch);
    return root;
}

std::unique_ptr<ImageFindBlock> makeImageFindBlock(const std::string& relativeTemplate,
                                                   bool returnToPrevious) {
    auto block = std::make_unique<ImageFindBlock>();
    block->templatePaths = {relativeTemplate};
    block->searchArea = SearchArea::FullScreen;
    block->pollIntervalMs = 0;
    block->threshold = 0.85;
    block->templateColorMode = TemplateColorMode::Grayscale;
    if (returnToPrevious) {
        block->returnToPreviousImageFindOnFailure = true;
        block->returnToPreviousMissLimit = 1;
        block->threshold = 0.99;
    }
    return block;
}

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

bool runImageFindMatchSuccess(const std::filesystem::path& projectRoot, const cv::Mat& haystack) {
    Workflow workflow;
    workflow.addBlock(makeImageFindBlock("templates/match.png", false));

    bool imageFindFinishedOk = false;
    WorkflowRunHooks hooks;
    hooks.onBlockFinished = [&](int blockIndex, bool success, const std::string& message, qint64,
                                qint64, int) {
        if (blockIndex == 0) {
            imageFindFinishedOk = success;
            if (!success) {
                std::cerr << "imagefind match block failed: " << message << '\n';
            }
        }
    };

    DryRunCaptureScope capture(haystack);
    ExecutionContext ctx;
    ctx.setProjectDirectory(projectRoot.generic_string());

    const WorkflowRunResult result = WorkflowRunner::run(workflow, ctx, &hooks);
    if (!result.success) {
        std::cerr << "imagefind match workflow failed: " << result.message << '\n';
        return false;
    }
    if (!ctx.hasLastMatch()) {
        std::cerr << "imagefind match: no last match recorded\n";
        return false;
    }
    if (!imageFindFinishedOk) {
        return false;
    }
    return true;
}

bool runImageFindReturnToPrevious(const std::filesystem::path& projectRoot, const cv::Mat& haystack) {
    Workflow workflow;
    workflow.addBlock(makeImageFindBlock("templates/match.png", false));
    auto wait = std::make_unique<WaitBlock>();
    wait->ms = 0;
    wait->randomRange = false;
    workflow.addBlock(std::move(wait));
    workflow.addBlock(makeImageFindBlock("templates/nomatch.png", true));

    DryRunCaptureScope capture(haystack);
    ExecutionContext ctx;
    ctx.setProjectDirectory(projectRoot.generic_string());

    int returnFrom = -1;
    int returnTo = -1;
    WorkflowRunHooks hooks;
    hooks.onImageFindReturnToPrevious = [&](int sourceBlockIndex, int targetBlockIndex) {
        returnFrom = sourceBlockIndex;
        returnTo = targetBlockIndex;
        ctx.requestStop();
    };

    const WorkflowRunResult result = WorkflowRunner::run(workflow, ctx, &hooks);
    if (returnFrom != 2 || returnTo != 0) {
        std::cerr << "return-to-previous expected 2->0, got " << returnFrom << "->" << returnTo
                  << '\n';
        return false;
    }
    (void)result;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
#ifdef _WIN32
    _putenv_s("PIPBONG_WORKFLOW_DRY_RUN", "1");
#endif

    if (!runWaitOnlyWorkflow(0)) {
        return 1;
    }
    if (!runLoopRegionExitsAfterOneIteration(LoopExitCondition::LastMatchFailed, false)) {
        return 1;
    }
    if (!runLoopRegionExitsAfterOneIteration(LoopExitCondition::LastMatchSuccess, true)) {
        return 1;
    }

    cv::Mat haystack;
    cv::Mat matchPatch;
    cv::Mat nomatchPatch;
    const std::filesystem::path projectRoot =
        prepareTemplateProject(haystack, matchPatch, nomatchPatch);

    if (!runImageFindMatchSuccess(projectRoot, haystack)) {
        return 1;
    }
    if (!runImageFindReturnToPrevious(projectRoot, haystack)) {
        return 1;
    }

    std::cout << "PIPBONGWorkflowDryRunSim: wait + loop-region + ImageFind branches OK (R6.2)\n";
    return 0;
}
