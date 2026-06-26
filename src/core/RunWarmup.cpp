#include "core/RunWarmup.h"

#include "core/capture/ScreenCapture.h"
#include "core/vision/ImageMatcher.h"
#include "core/workflow/Block.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/Feature.h"
#include "model/Project.h"

#include <unordered_set>

void RunWarmup::prefetch(const Project* project, const std::string& projectDirectory) {
#ifdef _WIN32
    ImageMatcher::warmup();
    ScreenCapture::warmupCapture();
#endif

    if (!project || projectDirectory.empty()) {
        return;
    }

    ExecutionContext resolveContext;
    resolveContext.setProjectDirectory(projectDirectory);

    std::unordered_set<std::string> loadedTemplates;
    for (const auto& feature : project->features()) {
        if (!feature) {
            continue;
        }
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
                if (loadedTemplates.insert(resolved).second) {
                    ImageMatcher::loadTemplate(resolved);
                }
            }
        }
    }
}
