#include "core/vision/ImageMatcher.h"



#include <opencv2/imgcodecs.hpp>

#include <opencv2/imgproc.hpp>



#include <algorithm>

#include <cmath>



namespace {



constexpr double kScaleEpsilon = 1e-6;

constexpr double kGrayscaleMaxMeanChannelSpread = 10.0;

cv::Mat toBgr(const cv::Mat& image) {
    if (image.empty()) {
        return {};
    }
    if (image.channels() == 3) {
        return image;
    }
    if (image.channels() == 4) {
        cv::Mat bgr;
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
        return bgr;
    }
    if (image.channels() == 1) {
        cv::Mat bgr;
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }
    return image;
}

double meanChannelSpread(const cv::Mat& image) {
    const cv::Mat bgr = toBgr(image);
    if (bgr.empty() || bgr.channels() < 3) {
        return 0.0;
    }

    std::vector<cv::Mat> channels(3);
    cv::split(bgr, channels);
    cv::Mat diff01;
    cv::Mat diff02;
    cv::Mat diff12;
    cv::Mat spread;
    cv::absdiff(channels[0], channels[1], diff01);
    cv::absdiff(channels[0], channels[2], diff02);
    cv::absdiff(channels[1], channels[2], diff12);
    cv::max(diff01, diff02, spread);
    cv::max(spread, diff12, spread);
    return cv::mean(spread)[0];
}

bool isWithinMatchBounds(const cv::Mat& haystack, const MatchResult& match) {
    if (haystack.empty() || !match.found) {
        return false;
    }
    const cv::Rect bounds(0, 0, haystack.cols, haystack.rows);
    const cv::Rect roi(match.location.x,
                       match.location.y,
                       match.matchedSize.width,
                       match.matchedSize.height);
    return (bounds & roi) == roi;
}

bool fitsInHaystack(const cv::Size& needleSize, const cv::Size& haystackSize) {

    return needleSize.width > 0 && needleSize.height > 0 && needleSize.width <= haystackSize.width &&

           needleSize.height <= haystackSize.height;

}



bool isWithinBounds(const cv::Point& topLeft, const cv::Size& size, const cv::Size& haystackSize) {

    if (topLeft.x < 0 || topLeft.y < 0) {

        return false;

    }

    return topLeft.x + size.width <= haystackSize.width && topLeft.y + size.height <= haystackSize.height;

}



cv::Rect matchRect(const MatchResult& match) {

    return cv::Rect(match.location.x, match.location.y, match.matchedSize.width, match.matchedSize.height);

}



bool rectanglesOverlap(const MatchResult& a, const MatchResult& b) {

    return (matchRect(a) & matchRect(b)).area() > 0;

}



bool topLeftBefore(const MatchResult& a, const MatchResult& b) {

    if (a.location.y != b.location.y) {

        return a.location.y < b.location.y;

    }

    return a.location.x < b.location.x;

}



std::vector<MatchResult> dedupeOverlappingTopLeftFirst(std::vector<MatchResult> matches) {

    std::sort(matches.begin(), matches.end(), topLeftBefore);

    std::vector<MatchResult> kept;

    kept.reserve(matches.size());

    for (const MatchResult& candidate : matches) {

        bool overlapsExisting = false;

        for (const MatchResult& existing : kept) {

            if (rectanglesOverlap(candidate, existing)) {

                overlapsExisting = true;

                break;

            }

        }

        if (!overlapsExisting) {

            kept.push_back(candidate);

        }

    }

    return kept;

}



MatchResult makeMatchResult(const cv::Point& maxLoc,

                            double maxVal,

                            double scale,

                            const cv::Size& needleSize,

                            const cv::Size& haystackSize) {

    MatchResult result;

    result.scale = scale;

    result.confidence = maxVal;

    result.location = maxLoc;

    result.matchedSize = needleSize;

    result.center = cv::Point(maxLoc.x + needleSize.width / 2, maxLoc.y + needleSize.height / 2);

    result.found = isWithinBounds(result.location, result.matchedSize, haystackSize);

    return result;

}



std::vector<MatchResult> collectMatchesAtScale(const cv::Mat& hayGray,

                                               const cv::Mat& needleGray,

                                               double scale,

                                               double threshold,

                                               bool enumerateAll) {

    std::vector<MatchResult> matches;

    if (!fitsInHaystack(needleGray.size(), hayGray.size())) {

        return matches;

    }



    cv::Mat correlation;

    cv::matchTemplate(hayGray, needleGray, correlation, cv::TM_CCOEFF_NORMED);



    if (!enumerateAll) {

        double minVal = 0.0;

        double maxVal = 0.0;

        cv::Point minLoc;

        cv::Point maxLoc;

        cv::minMaxLoc(correlation, &minVal, &maxVal, &minLoc, &maxLoc);

        if (ImageMatcher::meetsThreshold(maxVal, threshold)) {

            MatchResult result = makeMatchResult(maxLoc, maxVal, scale, needleGray.size(), hayGray.size());

            if (result.found) {

                matches.push_back(result);

            }

        }

        return matches;

    }



    cv::Mat suppressed = correlation.clone();

    while (true) {

        double minVal = 0.0;

        double maxVal = 0.0;

        cv::Point minLoc;

        cv::Point maxLoc;

        cv::minMaxLoc(suppressed, &minVal, &maxVal, &minLoc, &maxLoc);

        if (!ImageMatcher::meetsThreshold(maxVal, threshold)) {

            break;

        }



        MatchResult result = makeMatchResult(maxLoc, maxVal, scale, needleGray.size(), hayGray.size());

        if (result.found) {

            matches.push_back(result);

        }



        const cv::Rect suppressRect(maxLoc.x, maxLoc.y, needleGray.cols, needleGray.rows);

        cv::rectangle(suppressed, suppressRect, cv::Scalar(-1.0), cv::FILLED);

    }



    return matches;

}



MatchResult findPeakAtScale(const cv::Mat& hayGray, const cv::Mat& needleGray, double scale) {

    MatchResult result;

    if (!fitsInHaystack(needleGray.size(), hayGray.size())) {

        return result;

    }



    cv::Mat correlation;

    cv::matchTemplate(hayGray, needleGray, correlation, cv::TM_CCOEFF_NORMED);



    double minVal = 0.0;

    double maxVal = 0.0;

    cv::Point minLoc;

    cv::Point maxLoc;

    cv::minMaxLoc(correlation, &minVal, &maxVal, &minLoc, &maxLoc);



    return makeMatchResult(maxLoc, maxVal, scale, needleGray.size(), hayGray.size());

}



void appendUniqueScale(std::vector<double>& scales, double scale) {

    for (double existing : scales) {

        if (std::abs(existing - scale) < kScaleEpsilon) {

            return;

        }

    }

    scales.push_back(scale);

}



std::string scaleCacheKey(const MatchOptions& options) {

    if (!options.multiScale) {

        return "1";

    }

    return std::to_string(options.minScale) + ":" + std::to_string(options.maxScale);

}



} // namespace



bool ImageMatcher::meetsThreshold(double confidence, double threshold) {

    constexpr double kThresholdEpsilon = 1e-4;

    return confidence + kThresholdEpsilon >= threshold;

}



cv::Mat ImageMatcher::toGrayscale(const cv::Mat& image) {

    if (image.empty()) {

        return {};

    }



    cv::Mat gray;

    switch (image.channels()) {

    case 1:

        gray = image;

        break;

    case 4:

        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);

        break;

    default:

        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

        break;

    }

    return gray;

}



void ImageMatcher::toGrayscale(const cv::Mat& image, cv::Mat& grayOut) {

    if (image.empty()) {

        grayOut.release();

        return;

    }



    switch (image.channels()) {

    case 1:

        grayOut = image;

        break;

    case 4:

        if (grayOut.rows != image.rows || grayOut.cols != image.cols || grayOut.type() != CV_8UC1) {

            grayOut.create(image.rows, image.cols, CV_8UC1);

        }

        cv::cvtColor(image, grayOut, cv::COLOR_BGRA2GRAY);

        break;

    default:

        if (grayOut.rows != image.rows || grayOut.cols != image.cols || grayOut.type() != CV_8UC1) {

            grayOut.create(image.rows, image.cols, CV_8UC1);

        }

        cv::cvtColor(image, grayOut, cv::COLOR_BGR2GRAY);

        break;

    }

}



bool ImageMatcher::isEffectivelyGrayscale(const cv::Mat& image, double maxMeanChannelSpread) {

    if (image.empty() || image.channels() == 1) {

        return true;

    }

    return meanChannelSpread(image) <= maxMeanChannelSpread;

}



bool ImageMatcher::isMatchRegionGrayscale(const cv::Mat& haystack,

                                          const MatchResult& match,

                                          double maxMeanChannelSpread) {

    if (!isWithinMatchBounds(haystack, match)) {

        return false;

    }

    const cv::Rect roi(match.location.x,

                       match.location.y,

                       match.matchedSize.width,

                       match.matchedSize.height);

    return isEffectivelyGrayscale(haystack(roi), maxMeanChannelSpread);

}



std::vector<MatchResult> ImageMatcher::filterMatchesToGrayscaleHaystack(

    const cv::Mat& haystack,

    const std::vector<MatchResult>& matches,

    double maxMeanChannelSpread) {

    if (haystack.empty()) {

        return matches;

    }

    std::vector<MatchResult> filtered;

    filtered.reserve(matches.size());

    for (const MatchResult& match : matches) {

        if (isMatchRegionGrayscale(haystack, match, maxMeanChannelSpread)) {

            filtered.push_back(match);

        }

    }

    return filtered;

}

bool ImageMatcher::requiresGrayscaleHaystackRegion(TemplateColorMode mode, const PreparedTemplate& templ) {
    switch (mode) {
    case TemplateColorMode::Grayscale:
        return true;
    case TemplateColorMode::Color:
        return false;
    case TemplateColorMode::Auto:
    default:
        return templ.isGrayscaleTemplate;
    }
}

bool ImageMatcher::usesColorChannelMatching(TemplateColorMode mode, const PreparedTemplate& templ) {
    if (templ.isGrayscaleTemplate) {
        return false;
    }
    switch (mode) {
    case TemplateColorMode::Color:
    case TemplateColorMode::Auto:
        return true;
    case TemplateColorMode::Grayscale:
    default:
        return false;
    }
}

bool ImageMatcher::requiresColorHaystackRegion(TemplateColorMode mode, const PreparedTemplate& templ) {
    return usesColorChannelMatching(mode, templ);
}

std::vector<MatchResult> ImageMatcher::filterMatchesRejectingGrayscaleHaystack(
    const cv::Mat& haystack,
    const std::vector<MatchResult>& matches,
    double maxMeanChannelSpread) {
    if (haystack.empty()) {
        return matches;
    }

    std::vector<MatchResult> filtered;
    filtered.reserve(matches.size());
    for (const MatchResult& match : matches) {
        if (!isMatchRegionGrayscale(haystack, match, maxMeanChannelSpread)) {
            filtered.push_back(match);
        }
    }
    return filtered;
}

PreparedTemplate ImageMatcher::loadTemplate(const std::string& path) {

    PreparedTemplate templ;

    if (path.empty()) {

        return templ;

    }



    templ.bgr = cv::imread(path, cv::IMREAD_COLOR);

    if (templ.bgr.empty()) {

        templ.bgr = cv::imread(path, cv::IMREAD_UNCHANGED);

    }

    if (templ.bgr.empty()) {

        return templ;

    }



    templ.gray = toGrayscale(templ.bgr);

    templ.isGrayscaleTemplate = isEffectivelyGrayscale(templ.bgr, kGrayscaleMaxMeanChannelSpread);

    return templ;

}



void ImageMatcher::ensureTemplateScaleCache(const PreparedTemplate& templ, const MatchOptions& options) {

    const std::string key = scaleCacheKey(options);

    if (templ.scaleCacheKey == key && !templ.scaleCacheFactors.empty()) {

        return;

    }



    templ.scaleCacheKey = key;

    templ.scaleCacheFactors = scaleFactors(options);

    templ.scaleCacheGrays.clear();
    templ.scaleCacheBgrs.clear();

    templ.scaleCacheGrays.reserve(templ.scaleCacheFactors.size());



    for (const double scale : templ.scaleCacheFactors) {

        if (std::abs(scale - 1.0) < kScaleEpsilon) {

            templ.scaleCacheGrays.push_back(templ.gray);

            continue;

        }



        const int width = std::max(1, static_cast<int>(std::lround(templ.gray.cols * scale)));

        const int height = std::max(1, static_cast<int>(std::lround(templ.gray.rows * scale)));

        const int interpolation = scale < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR;

        cv::Mat resized;

        cv::resize(templ.gray, resized, cv::Size(width, height), 0, 0, interpolation);

        templ.scaleCacheGrays.push_back(std::move(resized));

    }

}



void ImageMatcher::ensureTemplateBgrScaleCache(const PreparedTemplate& templ, const MatchOptions& options) {

    ensureTemplateScaleCache(templ, options);

    if (!templ.scaleCacheBgrs.empty()
        && templ.scaleCacheBgrs.size() == templ.scaleCacheFactors.size()) {

        return;

    }



    templ.scaleCacheBgrs.clear();

    templ.scaleCacheBgrs.reserve(templ.scaleCacheFactors.size());

    const cv::Mat sourceBgr = toBgr(templ.bgr);



    for (const double scale : templ.scaleCacheFactors) {

        if (std::abs(scale - 1.0) < kScaleEpsilon) {

            templ.scaleCacheBgrs.push_back(sourceBgr);

            continue;

        }



        const int width = std::max(1, static_cast<int>(std::lround(sourceBgr.cols * scale)));

        const int height = std::max(1, static_cast<int>(std::lround(sourceBgr.rows * scale)));

        const int interpolation = scale < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR;

        cv::Mat resized;

        cv::resize(sourceBgr, resized, cv::Size(width, height), 0, 0, interpolation);

        templ.scaleCacheBgrs.push_back(std::move(resized));

    }

}



std::vector<double> ImageMatcher::scaleFactors(const MatchOptions& options) {

    std::vector<double> scales;

    if (!options.multiScale) {

        scales.push_back(1.0);

        return scales;

    }



    appendUniqueScale(scales, options.minScale);

    appendUniqueScale(scales, 1.0);

    appendUniqueScale(scales, options.maxScale);

    std::sort(scales.begin(), scales.end());

    return scales;

}



MatchResult ImageMatcher::findPeakMatchGray(const cv::Mat& hayGray,

                                            const PreparedTemplate& templ,

                                            const MatchOptions& options) {

    MatchResult best;

    if (hayGray.empty() || templ.empty()) {

        return best;

    }



    ensureTemplateScaleCache(templ, options);

    for (size_t i = 0; i < templ.scaleCacheFactors.size(); ++i) {

        const double scale = templ.scaleCacheFactors[i];

        const MatchResult peak = findPeakAtScale(hayGray, templ.scaleCacheGrays[i], scale);

        if (peak.found && peak.confidence > best.confidence) {

            best = peak;

        }

    }

    return best;

}



MatchResult ImageMatcher::findPeakMatchBgr(const cv::Mat& hayBgr,

                                           const PreparedTemplate& templ,

                                           const MatchOptions& options) {

    MatchResult best;

    const cv::Mat hay = toBgr(hayBgr);

    if (hay.empty() || templ.empty()) {

        return best;

    }



    ensureTemplateBgrScaleCache(templ, options);

    for (size_t i = 0; i < templ.scaleCacheFactors.size(); ++i) {

        const double scale = templ.scaleCacheFactors[i];

        const MatchResult peak = findPeakAtScale(hay, templ.scaleCacheBgrs[i], scale);

        if (peak.found && peak.confidence > best.confidence) {

            best = peak;

        }

    }

    return best;

}



void ImageMatcher::findPeakAndAllTemplatesGray(const cv::Mat& hayGray,

                                               const PreparedTemplate& templ,

                                               const MatchOptions& options,

                                               bool enumerateAll,

                                               MatchResult& outPeak,

                                               std::vector<MatchResult>& outMatches) {

    outPeak = {};

    outMatches.clear();

    if (hayGray.empty() || templ.empty()) {

        return;

    }



    ensureTemplateScaleCache(templ, options);

    for (size_t i = 0; i < templ.scaleCacheFactors.size(); ++i) {

        const double scale = templ.scaleCacheFactors[i];

        const cv::Mat& needleGray = templ.scaleCacheGrays[i];

        if (!fitsInHaystack(needleGray.size(), hayGray.size())) {

            continue;

        }



        cv::Mat correlation;

        cv::matchTemplate(hayGray, needleGray, correlation, cv::TM_CCOEFF_NORMED);



        double minVal = 0.0;

        double maxVal = 0.0;

        cv::Point minLoc;

        cv::Point maxLoc;

        cv::minMaxLoc(correlation, &minVal, &maxVal, &minLoc, &maxLoc);

        const MatchResult scalePeak =

            makeMatchResult(maxLoc, maxVal, scale, needleGray.size(), hayGray.size());

        if (scalePeak.found && scalePeak.confidence > outPeak.confidence) {

            outPeak = scalePeak;

        }



        if (!enumerateAll) {

            if (meetsThreshold(maxVal, options.threshold)) {

                MatchResult result =

                    makeMatchResult(maxLoc, maxVal, scale, needleGray.size(), hayGray.size());

                if (result.found) {

                    outMatches.push_back(result);

                }

            }

            continue;

        }



        cv::Mat suppressed = correlation.clone();

        while (true) {

            cv::minMaxLoc(suppressed, &minVal, &maxVal, &minLoc, &maxLoc);

            if (!meetsThreshold(maxVal, options.threshold)) {

                break;

            }



            MatchResult result =

                makeMatchResult(maxLoc, maxVal, scale, needleGray.size(), hayGray.size());

            if (result.found) {

                outMatches.push_back(result);

            }



            const cv::Rect suppressRect(maxLoc.x, maxLoc.y, needleGray.cols, needleGray.rows);

            cv::rectangle(suppressed, suppressRect, cv::Scalar(-1.0), cv::FILLED);

        }

    }



    if (enumerateAll) {

        outMatches = dedupeOverlappingTopLeftFirst(std::move(outMatches));

    }



    for (MatchResult& match : outMatches) {

        match.found = meetsThreshold(match.confidence, options.threshold) &&

                      isWithinBounds(match.location, match.matchedSize, hayGray.size());

    }



    outMatches.erase(std::remove_if(outMatches.begin(),

                                    outMatches.end(),

                                    [](const MatchResult& match) { return !match.found; }),

                     outMatches.end());

    std::sort(outMatches.begin(), outMatches.end(), topLeftBefore);

}



std::vector<MatchResult> ImageMatcher::findAllTemplatesGray(const cv::Mat& hayGray,

                                                            const PreparedTemplate& templ,

                                                            const MatchOptions& options,

                                                            bool enumerateAll) {

    std::vector<MatchResult> allMatches;

    if (hayGray.empty() || templ.empty()) {

        return allMatches;

    }



    ensureTemplateScaleCache(templ, options);

    for (size_t i = 0; i < templ.scaleCacheFactors.size(); ++i) {

        const double scale = templ.scaleCacheFactors[i];

        std::vector<MatchResult> scaleMatches = collectMatchesAtScale(

            hayGray, templ.scaleCacheGrays[i], scale, options.threshold, enumerateAll);

        allMatches.insert(allMatches.end(), scaleMatches.begin(), scaleMatches.end());

    }



    if (enumerateAll) {

        allMatches = dedupeOverlappingTopLeftFirst(std::move(allMatches));

    }



    for (MatchResult& match : allMatches) {

        match.found = meetsThreshold(match.confidence, options.threshold) &&

                      isWithinBounds(match.location, match.matchedSize, hayGray.size());

    }



    allMatches.erase(std::remove_if(allMatches.begin(),

                                    allMatches.end(),

                                    [](const MatchResult& match) { return !match.found; }),

                     allMatches.end());

    std::sort(allMatches.begin(), allMatches.end(), topLeftBefore);

    return allMatches;

}



std::vector<MatchResult> ImageMatcher::findAllTemplatesBgr(const cv::Mat& hayBgr,

                                                           const PreparedTemplate& templ,

                                                           const MatchOptions& options,

                                                           bool enumerateAll) {

    std::vector<MatchResult> allMatches;

    const cv::Mat hay = toBgr(hayBgr);

    if (hay.empty() || templ.empty()) {

        return allMatches;

    }



    ensureTemplateBgrScaleCache(templ, options);

    for (size_t i = 0; i < templ.scaleCacheFactors.size(); ++i) {

        const double scale = templ.scaleCacheFactors[i];

        std::vector<MatchResult> scaleMatches = collectMatchesAtScale(

            hay, templ.scaleCacheBgrs[i], scale, options.threshold, enumerateAll);

        allMatches.insert(allMatches.end(), scaleMatches.begin(), scaleMatches.end());

    }



    if (enumerateAll) {

        allMatches = dedupeOverlappingTopLeftFirst(std::move(allMatches));

    }



    for (MatchResult& match : allMatches) {

        match.found = meetsThreshold(match.confidence, options.threshold) &&

                      isWithinBounds(match.location, match.matchedSize, hay.size());

    }



    allMatches.erase(std::remove_if(allMatches.begin(),

                                    allMatches.end(),

                                    [](const MatchResult& match) { return !match.found; }),

                     allMatches.end());

    std::sort(allMatches.begin(), allMatches.end(), topLeftBefore);

    return allMatches;

}



MatchResult ImageMatcher::findTemplate(const cv::Mat& haystack,

                                       const PreparedTemplate& templ,

                                       const MatchOptions& options) {

    const std::vector<MatchResult> matches = findAllTemplates(haystack, templ, options);

    if (matches.empty()) {

        return {};

    }

    return matches.front();

}



std::vector<MatchResult> ImageMatcher::findAllTemplates(const cv::Mat& haystack,

                                                        const PreparedTemplate& templ,

                                                        const MatchOptions& options) {

    if (haystack.empty() || templ.empty()) {

        return {};

    }



    const cv::Mat hayGray = haystack.channels() == 1 ? haystack : toGrayscale(haystack);

    return findAllTemplatesGray(hayGray, templ, options, true);

}



MatchResult ImageMatcher::findPeakMatch(const cv::Mat& haystack,

                                        const PreparedTemplate& templ,

                                        const MatchOptions& options) {

    if (haystack.empty() || templ.empty()) {

        return {};

    }



    const cv::Mat hayGray = haystack.channels() == 1 ? haystack : toGrayscale(haystack);

    return findPeakMatchGray(hayGray, templ, options);

}

void ImageMatcher::warmup() {
    const cv::Mat hay(64, 64, CV_8UC1, cv::Scalar(128));
    const cv::Mat needle(16, 16, CV_8UC1, cv::Scalar(200));
    cv::Mat result;
    cv::matchTemplate(hay, needle, result, cv::TM_CCOEFF_NORMED);
}

