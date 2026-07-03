#pragma once

#include "core/workflow/Block.h"
#include "core/capture/ScreenCapture.h"
#include "core/vision/ImageMatcher.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ImageFindMatchTestResult {
    bool captureOk = false;
    /// Capture + template matching work time in milliseconds (single test attempt).
    int64_t matchDurationMs = -1;
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

constexpr int kDefaultRoiCorrectionExpandPercent = 110;
constexpr int kRoiCorrectionExpandPercentMin = 50;
constexpr int kRoiCorrectionExpandPercentMax = 300;
constexpr int kRoiCorrectionExpandPercentStep = 5;
int snapRoiCorrectionExpandPercent(int percent);

class ImageFindBlock : public Block {
public:
    std::vector<std::string> templatePaths;
    ImageFindTemplateMatchMode templateMatchMode = ImageFindTemplateMatchMode::Any;
    TemplateColorMode templateColorMode = TemplateColorMode::Auto;
    double threshold = 0.85;
    bool multiScale = false;
    double minScale = 0.9;
    double maxScale = 1.1;
    int pollIntervalMs = kDefaultImageFindPollIntervalMs;
    SearchArea searchArea = SearchArea::TargetWindow;
    /// Legacy screen-pixel ROIs; migrated to `customRegionsWindowPercent` on load.
    std::vector<CaptureRegion> customRegions;
    /// Legacy single ROI; not written on save.
    CaptureRegion customRegion;
    /// Always true for saved projects; ROIs are % of target window (DWM bounds).
    bool customRegionsAnchoredToTargetWindow = true;
    std::vector<PercentRegion> customRegionsWindowPercent;
    PercentRegion percentRegion;
    /// When feature-level ROI correction is off, enables session ROI correction for this block only.
    bool roiCorrection = false;
    /// Loop 2+ corrected search ROI size as % of matched template (100 = same size, 110 = 10% wider/taller).
    int roiCorrectionExpandPercent = kDefaultRoiCorrectionExpandPercent;
    /// On detection failure (miss limit), workflow jumps to the previous ImageFind block.
    bool returnToPreviousImageFindOnFailure = false;
    /// On detection failure: run the next block once, retry this block; on second failure jump to next ImageFind.
    bool retryAfterNextActionOnFailure = false;

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
                                              const PercentRegion& percentRegion,
                                              const std::string& templatePath,
                                              const MatchOptions& options,
                                              const std::string& projectDirectory,
                                              const std::vector<PercentRegion>& customRegionsWindowPercent =
                                                  {});

    static ImageFindMatchTestResult testMatchTemplates(SearchArea searchArea,
                                                       const PercentRegion& percentRegion,
                                                       const std::vector<std::string>& templatePaths,
                                                       const MatchOptions& options,
                                                       const std::string& projectDirectory,
                                                       const std::vector<PercentRegion>& customRegionsWindowPercent =
                                                           {});

private:
    const PreparedTemplate& cachedTemplateFor(const std::string& resolvedPath) const;

    mutable std::unordered_map<std::string, PreparedTemplate> m_cachedTemplates;
};

