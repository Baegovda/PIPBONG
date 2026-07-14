#include "core/RunWarmup.h"

#include "core/capture/ScreenCapture.h"
#include "core/vision/ImageMatcher.h"
#include "core/vision/TemplateCache.h"
#include "core/workflow/Block.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/Feature.h"
#include "model/Project.h"

#include <unordered_set>
#include <vector>

namespace {

void collectTemplatePaths(const Project* project,
                            const std::string& projectDirectory,
                            const std::string& priorityFeatureId,
                            std::vector<std::string>& priorityPaths,
                            std::vector<std::string>& otherPaths) {
    if (!project || projectDirectory.empty()) {
        return;
    }

    ExecutionContext resolveContext;
    resolveContext.setProjectDirectory(projectDirectory);

    std::unordered_set<std::string> seen;
    for (const auto& feature : project->features()) {
        if (!feature) {
            continue;
        }
        const bool isPriority = !priorityFeatureId.empty() && feature->id() == priorityFeatureId;
        for (const auto& block : feature->workflow().blocks()) {
            if (!block || block->type() != BlockType::ImageFind) {
                continue;
            }
            const auto* imageFind = static_cast<const ImageFindBlock*>(block.get());
            for (const std::string& templatePath : imageFind->templatePaths) {
                if (templatePath.empty()) {
                    continue;
                }
                const std::string resolved = resolveContext.resolvePath(templatePath);
                if (!seen.insert(resolved).second) {
                    continue;
                }
                if (isPriority) {
                    priorityPaths.push_back(resolved);
                } else {
                    otherPaths.push_back(resolved);
                }
            }
        }
    }
}

} // namespace

void RunWarmup::prefetch(const Project* project, const std::string& projectDirectory) {
    prefetch(project, projectDirectory, {});
}

void RunWarmup::prefetch(const Project* project,
                         const std::string& projectDirectory,
                         const std::string& priorityFeatureId) {
#ifdef _WIN32
    static bool s_captureWarmed = false;
    ImageMatcher::warmup();
    if (!s_captureWarmed) {
        ScreenCapture::warmupCapture();
        s_captureWarmed = true;
    }
#endif

    std::vector<std::string> priorityPaths;
    std::vector<std::string> otherPaths;
    collectTemplatePaths(project, projectDirectory, priorityFeatureId, priorityPaths, otherPaths);

    for (const std::string& path : priorityPaths) {
        (void)TemplateCache::getOrLoad(path);
    }

    if (!otherPaths.empty()) {
        TemplateCache::prefetchAsync(otherPaths);
    }
}
