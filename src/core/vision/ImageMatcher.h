#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <vector>

struct MatchResult {
    cv::Point location;
    cv::Point center;
    cv::Size matchedSize;
    double confidence = 0.0;
    double scale = 1.0;
    bool found = false;
};

enum class TemplateColorMode {
    Auto,
    Grayscale,
    Color
};

struct MatchOptions {
    double threshold = 0.85;
    bool multiScale = false;
    double minScale = 0.9;
    double maxScale = 1.1;
    TemplateColorMode templateColorMode = TemplateColorMode::Auto;
};

struct PreparedTemplate {
    cv::Mat bgr;
    cv::Mat gray;
    bool isGrayscaleTemplate = false;
    mutable std::string scaleCacheKey;
    mutable std::vector<double> scaleCacheFactors;
    mutable std::vector<cv::Mat> scaleCacheGrays;
    mutable std::vector<cv::Mat> scaleCacheBgrs;

    bool empty() const { return bgr.empty() || gray.empty(); }
};

class ImageMatcher {
public:
    static bool meetsThreshold(double confidence, double threshold);

    static PreparedTemplate loadTemplate(const std::string& path);
    static cv::Mat toGrayscale(const cv::Mat& image);
    static void toGrayscale(const cv::Mat& image, cv::Mat& grayOut);
    static bool isEffectivelyGrayscale(const cv::Mat& image, double maxMeanChannelSpread = 10.0);
    static bool isMatchRegionGrayscale(const cv::Mat& haystack,
                                       const MatchResult& match,
                                       double maxMeanChannelSpread = 10.0);
    static std::vector<MatchResult> filterMatchesToGrayscaleHaystack(
        const cv::Mat& haystack,
        const std::vector<MatchResult>& matches,
        double maxMeanChannelSpread = 10.0);
    static bool requiresGrayscaleHaystackRegion(TemplateColorMode mode, const PreparedTemplate& templ);
    static bool requiresColorHaystackRegion(TemplateColorMode mode, const PreparedTemplate& templ);
    static bool usesColorChannelMatching(TemplateColorMode mode, const PreparedTemplate& templ);
    static std::vector<MatchResult> filterMatchesRejectingGrayscaleHaystack(
        const cv::Mat& haystack,
        const std::vector<MatchResult>& matches,
        double maxMeanChannelSpread = 10.0);
    static MatchResult findTemplate(const cv::Mat& haystack,
                                    const PreparedTemplate& templ,
                                    const MatchOptions& options = {});
    static std::vector<MatchResult> findAllTemplates(const cv::Mat& haystack,
                                                     const PreparedTemplate& templ,
                                                     const MatchOptions& options = {});
    static MatchResult findPeakMatch(const cv::Mat& haystack,
                                     const PreparedTemplate& templ,
                                     const MatchOptions& options = {});

    static MatchResult findPeakMatchGray(const cv::Mat& hayGray,
                                         const PreparedTemplate& templ,
                                         const MatchOptions& options);
    static MatchResult findPeakMatchBgr(const cv::Mat& hayBgr,
                                        const PreparedTemplate& templ,
                                        const MatchOptions& options);
    static void findPeakAndAllTemplatesGray(const cv::Mat& hayGray,
                                            const PreparedTemplate& templ,
                                            const MatchOptions& options,
                                            bool enumerateAll,
                                            MatchResult& outPeak,
                                            std::vector<MatchResult>& outMatches);
    static void warmup();
    static std::vector<MatchResult> findAllTemplatesGray(const cv::Mat& hayGray,
                                                         const PreparedTemplate& templ,
                                                         const MatchOptions& options,
                                                         bool enumerateAll = true);
    static std::vector<MatchResult> findAllTemplatesBgr(const cv::Mat& hayBgr,
                                                        const PreparedTemplate& templ,
                                                        const MatchOptions& options,
                                                        bool enumerateAll = true);

    static std::vector<double> scaleFactors(const MatchOptions& options);

private:
    static void ensureTemplateScaleCache(const PreparedTemplate& templ, const MatchOptions& options);
    static void ensureTemplateBgrScaleCache(const PreparedTemplate& templ, const MatchOptions& options);
};
