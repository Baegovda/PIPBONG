#pragma once

#include "core/workflow/Block.h"
#include "core/capture/ScreenCapture.h"
#include "core/vision/ImageMatcher.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ImageFindMatchTestResult {
    bool captureOk = false;
    std::string errorMessage;
    cv::Mat haystack;
    cv::Mat needle;
    MatchResult match;
    std::vector<MatchResult> matches;
    /// Populated when multiple custom ROIs are searched; each entry is haystack-local matches.
    std::vector<std::pair<CaptureRegion, std::vector<MatchResult>>> matchesPerCustomRegion;
};

enum class ImageFindTemplateMatchMode {
    Any,
    All
};

constexpr int kImageFindPollIntervalStepMs = 5;
constexpr int kDefaultImageFindPollIntervalMs = 200;
int snapImageFindPollIntervalMs(int ms);

class ImageFindBlock : public Block {
public:
    std::vector<std::string> templatePaths;
    ImageFindTemplateMatchMode templateMatchMode = ImageFindTemplateMatchMode::Any;
    double threshold = 0.85;
    bool multiScale = false;
    double minScale = 0.9;
    double maxScale = 1.1;
    int pollIntervalMs = kDefaultImageFindPollIntervalMs;
    SearchArea searchArea = SearchArea::TargetWindow;
    /// Screen-pixel search ROIs when `searchArea` is `CustomRegion` (order = try order).
    std::vector<CaptureRegion> customRegions;
    /// Legacy single ROI; kept in sync with `customRegions.front()` when the list is non-empty.
    CaptureRegion customRegion;
    PercentRegion percentRegion;
    /// When feature-level ROI correction is off, enables session ROI correction for this block only.
    bool roiCorrection = false;

    bool hasTemplates() const;
    std::string primaryTemplatePath() const;

    MatchOptions matchOptions() const;

    BlockType type() const override { return BlockType::ImageFind; }
    std::string displayName() const override { return "ImageFind"; }
    std::string summary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<ImageFindBlock> fromJson(const nlohmann::json& json);

    static ImageFindMatchTestResult testMatch(SearchArea searchArea,
                                              const CaptureRegion& customRegion,
                                              const PercentRegion& percentRegion,
                                              const std::string& templatePath,
                                              const MatchOptions& options,
                                              const std::string& projectDirectory);

    static ImageFindMatchTestResult testMatchTemplates(SearchArea searchArea,
                                                       const CaptureRegion& customRegion,
                                                       const PercentRegion& percentRegion,
                                                       const std::vector<CaptureRegion>& customRegions,
                                                       const std::vector<std::string>& templatePaths,
                                                       const MatchOptions& options,
                                                       const std::string& projectDirectory);

private:
    const PreparedTemplate& cachedTemplateFor(const std::string& resolvedPath) const;

    mutable std::unordered_map<std::string, PreparedTemplate> m_cachedTemplates;
};

