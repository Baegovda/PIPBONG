#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/input/HotkeyBinding.h"
#include "core/vision/ImageMatcher.h"
#include "core/workflow/ExecutionContext.h"
#include "ui/editors/WorkflowMatchFeedbackOverlay.h"
#include "ui/editors/WorkflowRoiFlashOverlay.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <thread>

int snapImageFindPollIntervalMs(int ms) {
    ms = std::max(kImageFindPollIntervalStepMs, ms);
    return ((ms + kImageFindPollIntervalStepMs / 2) / kImageFindPollIntervalStepMs)
           * kImageFindPollIntervalStepMs;
}

int snapRoiCorrectionExpandPercent(int percent) {
    percent = std::clamp(percent, kRoiCorrectionExpandPercentMin, kRoiCorrectionExpandPercentMax);
    return ((percent + kRoiCorrectionExpandPercentStep / 2) / kRoiCorrectionExpandPercentStep)
           * kRoiCorrectionExpandPercentStep;
}

namespace {
bool sleepUnlessStopped(ExecutionContext& ctx, int delayMs) {
    return ctx.interruptibleSleepMs(delayMs);
}

struct RunRoiOverlayGuard {
    ~RunRoiOverlayGuard() { WorkflowRoiFlashOverlay::dismissAll(); }
};

SearchArea searchAreaFromString(const std::string& value) {
    if (value == "FullScreen") return SearchArea::FullScreen;
    if (value == "CustomRegion") return SearchArea::CustomRegion;
    if (value == "ScreenPercent") return SearchArea::ScreenPercent;
    return SearchArea::TargetWindow;
}

std::string searchAreaToString(SearchArea area) {
    switch (area) {
    case SearchArea::FullScreen:
        return "FullScreen";
    case SearchArea::CustomRegion:
        return "CustomRegion";
    case SearchArea::ScreenPercent:
        return "ScreenPercent";
    case SearchArea::TargetWindow:
    default:
        return "TargetWindow";
    }
}

std::string resolveTemplatePath(const std::string& templatePath, const std::string& projectDirectory) {
    if (templatePath.empty()) {
        return templatePath;
    }
    const std::filesystem::path path(templatePath);
    if (path.is_absolute()) {
        return templatePath;
    }
    if (projectDirectory.empty()) {
        return templatePath;
    }
    return (std::filesystem::path(projectDirectory) / path).string();
}

int countDistinctMatchInstances(const std::vector<MatchResult>& matches,
                                SearchArea searchArea,
                                const CaptureRegion& customRegion,
                                const PercentRegion& percentRegion) {
    std::vector<cv::Rect> physicalRects;
    physicalRects.reserve(matches.size());
    for (const MatchResult& match : matches) {
        if (!match.found) {
            continue;
        }
        const cv::Point physicalTopLeft = ScreenCapture::haystackTopLeftToPhysical(
            searchArea, customRegion, percentRegion, match.location);
        const cv::Rect physicalRect(physicalTopLeft.x,
                                      physicalTopLeft.y,
                                      match.matchedSize.width,
                                      match.matchedSize.height);
        bool overlapsExisting = false;
        for (const cv::Rect& existing : physicalRects) {
            if ((physicalRect & existing).area() > 0) {
                overlapsExisting = true;
                break;
            }
        }
        if (!overlapsExisting) {
            physicalRects.push_back(physicalRect);
        }
    }
    return static_cast<int>(physicalRects.size());
}

const MatchResult* selectNextTopLeftMatch(const std::vector<MatchResult>& matches,
                                          ExecutionContext& ctx,
                                          SearchArea searchArea,
                                          const CaptureRegion& customRegion,
                                          const PercentRegion& percentRegion,
                                          bool enforceConsumedSkip) {
    for (const MatchResult& candidate : matches) {
        if (!candidate.found) {
            continue;
        }
        if (!enforceConsumedSkip) {
            return &candidate;
        }
        const cv::Point physicalTopLeft = ScreenCapture::haystackTopLeftToPhysical(
            searchArea, customRegion, percentRegion, candidate.location);
        if (!ctx.isMatchRegionConsumed(physicalTopLeft, candidate.matchedSize)) {
            return &candidate;
        }
    }
    return nullptr;
}

MatchResult bestMatchByConfidence(const std::vector<MatchResult>& matches) {
    const MatchResult* best = nullptr;
    for (const MatchResult& match : matches) {
        if (!match.found) {
            continue;
        }
        if (!best || match.confidence > best->confidence) {
            best = &match;
        }
    }
    return best ? *best : MatchResult{};
}

void applyGrayscaleHaystackFilter(const cv::Mat& haystack,
                                  bool requireGrayscaleHaystack,
                                  std::vector<MatchResult>& matches,
                                  MatchResult& peak) {
    if (!requireGrayscaleHaystack || haystack.empty()) {
        return;
    }

    matches = ImageMatcher::filterMatchesToGrayscaleHaystack(haystack, matches);
    if (matches.empty()) {
        if (peak.found && !ImageMatcher::isMatchRegionGrayscale(haystack, peak)) {
            peak = {};
        }
        return;
    }

    peak = bestMatchByConfidence(matches);
}

struct ImageFindSelection {
    MatchResult match;
    std::vector<MatchResult> allMatches;
    MatchResult peak;
    bool ok = false;
};

ImageFindSelection trySelectImageFindMatch(const cv::Mat& haystack,
                                           const cv::Mat& hayGray,
                                           const PreparedTemplate& templ,
                                           const MatchOptions& options,
                                           ExecutionContext& ctx,
                                           SearchArea searchArea,
                                           const CaptureRegion& customRegion,
                                           const PercentRegion& percentRegion) {
    ImageFindSelection selection;
    if (hayGray.empty()) {
        return selection;
    }

    const bool requireGrayscaleHaystack =
        ImageMatcher::requiresGrayscaleHaystackRegion(options.templateColorMode, templ);

    selection.peak = ImageMatcher::findPeakMatchGray(hayGray, templ, options);
    if (!selection.peak.found
        || !ImageMatcher::meetsThreshold(selection.peak.confidence, options.threshold)) {
        return selection;
    }

    if (!ctx.hasConsumedMatchRegions()) {
        if (requireGrayscaleHaystack
            && !ImageMatcher::isMatchRegionGrayscale(haystack, selection.peak)) {
            return selection;
        }
        selection.match = selection.peak;
        selection.ok = true;
        return selection;
    }

    selection.allMatches =
        ImageMatcher::findAllTemplatesGray(hayGray, templ, options, true);
    applyGrayscaleHaystackFilter(haystack, requireGrayscaleHaystack, selection.allMatches, selection.peak);
    if (selection.allMatches.empty()) {
        return selection;
    }

    const bool enforceConsumedSkip =
        ctx.hasConsumedMatchRegions()
        && countDistinctMatchInstances(selection.allMatches, searchArea, customRegion, percentRegion) > 1;

    if (const MatchResult* selected = selectNextTopLeftMatch(
            selection.allMatches, ctx, searchArea, customRegion, percentRegion, enforceConsumedSkip)) {
        selection.match = *selected;
        selection.ok = true;
        return selection;
    }

    if (!selection.allMatches.empty()) {
        ctx.log("매칭 후보가 있으나 모두 이미 사용한 영역입니다.");
    } else {
        ctx.log("임계값 이상이나 이미 사용한 매칭 영역입니다.");
    }
    return selection;
}

bool isValidCustomRegion(const CaptureRegion& region) {
    return region.width >= 2 && region.height >= 2;
}

bool isValidWindowPercentRegion(const PercentRegion& region) {
    return region.width > 0.0 && region.height > 0.0;
}

void normalizeImageFindSearchArea(SearchArea& searchArea,
                                  const std::vector<PercentRegion>& windowPercentRegions) {
    for (const PercentRegion& region : windowPercentRegions) {
        if (isValidWindowPercentRegion(region)) {
            searchArea = SearchArea::CustomRegion;
            return;
        }
    }
}

void pruneInvalidWindowPercentRegions(std::vector<PercentRegion>& regions) {
    regions.erase(std::remove_if(regions.begin(),
                                 regions.end(),
                                 [](const PercentRegion& region) {
                                     return !isValidWindowPercentRegion(region);
                                 }),
                  regions.end());
}

PercentRegion percentRegionFromJson(const nlohmann::json& region) {
    PercentRegion out;
    out.x = region.value("x", 0.0);
    out.y = region.value("y", 0.0);
    out.width = region.value("width", 0.0);
    out.height = region.value("height", 0.0);
    return out;
}

nlohmann::json percentRegionToJson(const PercentRegion& region) {
    return nlohmann::json{{"x", region.x},
                          {"y", region.y},
                          {"width", region.width},
                          {"height", region.height}};
}

bool anchoredJsonLooksLikePixelOffset(const nlohmann::json& region) {
    const auto asNumber = [&](const char* key) -> double {
        if (!region.contains(key) || !region[key].is_number()) {
            return 0.0;
        }
        return region[key].get<double>();
    };
    const double width = asNumber("width");
    const double height = asNumber("height");
    const double x = asNumber("x");
    const double y = asNumber("y");
    return width > 100.0 || height > 100.0 || x > 100.0 || y > 100.0;
}

PercentRegion pixelOffsetToWindowPercent(const CaptureRegion& offset) {
    PercentRegion percent;
#ifdef _WIN32
    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0 || offset.width <= 0
        || offset.height <= 0) {
        return percent;
    }
    percent.x = offset.x * 100.0 / target.width;
    percent.y = offset.y * 100.0 / target.height;
    percent.width = offset.width * 100.0 / target.width;
    percent.height = offset.height * 100.0 / target.height;
#endif
    return percent;
}

PercentRegion expandWindowPercentRegion(const PercentRegion& matched, int expandPercent) {
    PercentRegion region = matched;
    if (region.width <= 0.0 || region.height <= 0.0) {
        return region;
    }
    const double ratio = static_cast<double>(snapRoiCorrectionExpandPercent(expandPercent)) / 100.0;
    double expandedWidth = region.width * ratio;
    double expandedHeight = region.height * ratio;
    if (expandedWidth < region.width + 0.01) {
        expandedWidth = region.width + 0.01;
    }
    if (expandedHeight < region.height + 0.01) {
        expandedHeight = region.height + 0.01;
    }
    const double centerX = region.x + region.width / 2.0;
    const double centerY = region.y + region.height / 2.0;
    region.width = expandedWidth;
    region.height = expandedHeight;
    region.x = centerX - expandedWidth / 2.0;
    region.y = centerY - expandedHeight / 2.0;
    return region;
}

void pruneInvalidCustomRegions(std::vector<CaptureRegion>& regions) {
    regions.erase(std::remove_if(regions.begin(),
                                 regions.end(),
                                 [](const CaptureRegion& region) {
                                     return !isValidCustomRegion(region);
                                 }),
                  regions.end());
}

std::vector<CaptureRegion> effectiveCustomRegions(const std::vector<CaptureRegion>& customRegions,
                                                  const CaptureRegion& legacyCustomRegion) {
    std::vector<CaptureRegion> regions = customRegions;
    pruneInvalidCustomRegions(regions);
    if (regions.empty() && isValidCustomRegion(legacyCustomRegion)) {
        regions.push_back(legacyCustomRegion);
    }
    return regions;
}

std::vector<CaptureRegion> physicalCustomPollRegions(
    SearchArea searchArea,
    const std::vector<PercentRegion>& windowPercentRegions) {
    std::vector<CaptureRegion> pollRegions;
    if (searchArea == SearchArea::CustomRegion) {
        pollRegions.reserve(windowPercentRegions.size());
        for (const PercentRegion& percent : windowPercentRegions) {
            if (!isValidWindowPercentRegion(percent)) {
                continue;
            }
            pollRegions.push_back(ScreenCapture::resolveWindowPercentRegion(percent));
        }
    } else {
        pollRegions.assign(1, CaptureRegion{});
    }
    return pollRegions;
}

void syncLegacyCustomRegionFromList(CaptureRegion& customRegion,
                                    const std::vector<CaptureRegion>& customRegions) {
    if (!customRegions.empty()) {
        customRegion = customRegions.front();
    }
}

CaptureRegion captureRegionFromJson(const nlohmann::json& region) {
    CaptureRegion out;
    out.x = region.value("x", 0);
    out.y = region.value("y", 0);
    out.width = region.value("width", 0);
    out.height = region.value("height", 0);
    return out;
}

nlohmann::json captureRegionToJson(const CaptureRegion& region) {
    return nlohmann::json{{"x", region.x},
                          {"y", region.y},
                          {"width", region.width},
                          {"height", region.height}};
}

ImageFindTemplateMatchMode templateMatchModeFromString(const std::string& value) {
    if (value == "All") {
        return ImageFindTemplateMatchMode::All;
    }
    return ImageFindTemplateMatchMode::Any;
}

std::string templateMatchModeToString(ImageFindTemplateMatchMode mode) {
    switch (mode) {
    case ImageFindTemplateMatchMode::All:
        return "All";
    case ImageFindTemplateMatchMode::Any:
    default:
        return "Any";
    }
}

TemplateColorMode templateColorModeFromString(const std::string& value) {
    if (value == "Grayscale") {
        return TemplateColorMode::Grayscale;
    }
    if (value == "Color") {
        return TemplateColorMode::Color;
    }
    return TemplateColorMode::Auto;
}

std::string templateColorModeToString(TemplateColorMode mode) {
    switch (mode) {
    case TemplateColorMode::Grayscale:
        return "Grayscale";
    case TemplateColorMode::Color:
        return "Color";
    case TemplateColorMode::Auto:
    default:
        return "Auto";
    }
}

void pruneEmptyTemplatePaths(std::vector<std::string>& paths) {
    paths.erase(std::remove_if(paths.begin(),
                               paths.end(),
                               [](const std::string& path) { return path.empty(); }),
                paths.end());
}

cv::Point matchCenterToClickPoint(SearchArea searchArea,
                                  const CaptureRegion& customRegion,
                                  const PercentRegion& percentRegion,
                                  const cv::Point& haystackCenter,
                                  HWND targetWindow,
                                  cv::Point* screenPointOut = nullptr,
                                  bool* hasScreenPointOut = nullptr) {
    if (hasScreenPointOut) {
        *hasScreenPointOut = false;
    }
    cv::Point clickPoint = haystackCenter;
#ifdef _WIN32
    int screenX = haystackCenter.x;
    int screenY = haystackCenter.y;

    switch (searchArea) {
    case SearchArea::TargetWindow:
#ifdef _WIN32
        if (targetWindow) {
            const ScreenCapture::WindowBounds bounds = ScreenCapture::physicalRectForWindow(targetWindow);
            if (bounds.valid) {
                screenX = bounds.x + haystackCenter.x;
                screenY = bounds.y + haystackCenter.y;
                if (screenPointOut) {
                    *screenPointOut = cv::Point(screenX, screenY);
                }
                if (hasScreenPointOut) {
                    *hasScreenPointOut = true;
                }
                POINT pt{screenX, screenY};
                if (ScreenToClient(targetWindow, &pt)) {
                    return cv::Point(pt.x, pt.y);
                }
            }
        }
#endif
        return clickPoint;
    case SearchArea::CustomRegion:
        screenX += customRegion.x;
        screenY += customRegion.y;
        break;
    case SearchArea::ScreenPercent: {
        const CaptureRegion relative = ScreenCapture::resolveWindowPercentRegion(percentRegion);
        screenX = relative.x + haystackCenter.x;
        screenY = relative.y + haystackCenter.y;
        break;
    }
    case SearchArea::FullScreen:
        screenX += GetSystemMetrics(SM_XVIRTUALSCREEN);
        screenY += GetSystemMetrics(SM_YVIRTUALSCREEN);
        break;
    }

    if (targetWindow) {
        POINT pt{screenX, screenY};
        if (ScreenToClient(targetWindow, &pt)) {
            clickPoint = cv::Point(pt.x, pt.y);
        } else {
            clickPoint = cv::Point(screenX, screenY);
        }
    } else {
        clickPoint = cv::Point(screenX, screenY);
    }
    if (screenPointOut) {
        *screenPointOut = cv::Point(screenX, screenY);
    }
    if (hasScreenPointOut) {
        *hasScreenPointOut = true;
    }
#else
    (void)searchArea;
    (void)customRegion;
    (void)percentRegion;
    (void)targetWindow;
#endif
    return clickPoint;
}

void reportImageFindMiss(ExecutionContext& ctx,
                         double matchThreshold,
                         const MatchResult& peak,
                         SearchArea searchArea,
                         const CaptureRegion& customRegion,
                         const PercentRegion& percentRegion,
                         HWND targetWindow) {
    const double peakConfidence = peak.found ? peak.confidence : 0.0;
    cv::Point clientPoint;
    bool hasClientPoint = false;
    if (peak.found) {
        clientPoint =
            matchCenterToClickPoint(searchArea, customRegion, percentRegion, peak.center, targetWindow);
        hasClientPoint = true;
    }
    ctx.setLastMatchAttempt(matchThreshold, peakConfidence, clientPoint, hasClientPoint);
    ctx.reportProgress(BlockProgressKind::ImageFindMiss);
}

bool imageFindExceededMissLimit(ExecutionContext& ctx, int& missAttempts) {
    const int maxMisses = ctx.imageFindMaxMissAttempts();
    if (maxMisses <= 0) {
        return false;
    }
    ++missAttempts;
    return missAttempts >= maxMisses;
}

BlockResult imageFindDetectionFailureResult(ExecutionContext& ctx) {
    ctx.markDetectionFailure();
    BlockResult result;
    result.success = false;
    result.message = "감지 실패";
    return result;
}

void maybeRecordRoiCorrection(ExecutionContext& ctx,
                              bool blockRoiCorrection,
                              int roiCorrectionExpandPercent,
                              const MatchResult& match,
                              SearchArea searchArea,
                              const CaptureRegion& customRegion,
                              const PercentRegion& percentRegion) {
    if (!ctx.shouldUseRoiCorrectionForBlock(blockRoiCorrection) || ctx.runLoopNumber() != 1) {
        return;
    }
    const int blockIndex = ctx.activeBlockIndex();
    if (blockIndex < 0 || match.matchedSize.width <= 0 || match.matchedSize.height <= 0) {
        return;
    }
    const cv::Point physicalTopLeft = ScreenCapture::haystackTopLeftToPhysical(
        searchArea, customRegion, percentRegion, match.location);
    CaptureRegion physical;
    physical.x = physicalTopLeft.x;
    physical.y = physicalTopLeft.y;
    physical.width = match.matchedSize.width;
    physical.height = match.matchedSize.height;
    PercentRegion percent = ScreenCapture::storeWindowPercentFromPhysical(physical);
    if (!isValidWindowPercentRegion(percent)) {
        return;
    }
    percent = expandWindowPercentRegion(percent, roiCorrectionExpandPercent);
    ctx.setCorrectedRoi(blockIndex, percent);
}

BlockResult imageFindSuccessResult(ExecutionContext& ctx,
                                   bool blockRoiCorrection,
                                   int roiCorrectionExpandPercent,
                                   const cv::Mat& haystack,
                                   const ImageFindSelection& selection,
                                   SearchArea searchArea,
                                   const CaptureRegion& customRegion,
                                   const PercentRegion& percentRegion,
                                   HWND targetWindow,
                                   double matchThreshold,
                                   const std::string& matchedTemplatePath,
                                   bool consumeRegion = true,
                                   const std::string& extraMessageSuffix = "") {
    const MatchResult& match = selection.match;
    cv::Mat preview;
    if (match.matchedSize.width > 0 && match.matchedSize.height > 0) {
        const cv::Rect roi(match.location.x,
                           match.location.y,
                           match.matchedSize.width,
                           match.matchedSize.height);
        const cv::Rect bounds(0, 0, haystack.cols, haystack.rows);
        const cv::Rect safe = roi & bounds;
        if (safe.area() > 0) {
            preview = haystack(safe).clone();
        }
    }

    cv::Point matchScreenPoint;
    bool hasMatchScreenPoint = false;
    const cv::Point matchPoint =
        matchCenterToClickPoint(searchArea, customRegion, percentRegion, match.center, targetWindow,
                                &matchScreenPoint, &hasMatchScreenPoint);

    if (consumeRegion) {
        ctx.consumeMatchRegion(
            ScreenCapture::haystackTopLeftToPhysical(
                searchArea, customRegion, percentRegion, match.location),
            match.matchedSize);
    }
    ctx.setLastMatch(matchPoint, match.confidence, preview, matchThreshold);
    if (hasMatchScreenPoint) {
        ctx.setLastMatchScreenPoint(matchScreenPoint);
    }

    int foundCount = 0;
    int selectedIndex = 0;
    for (const MatchResult& candidate : selection.allMatches) {
        if (!candidate.found) {
            continue;
        }
        ++foundCount;
        if (candidate.location == match.location && candidate.matchedSize == match.matchedSize) {
            selectedIndex = foundCount;
        }
    }
    if (foundCount == 0) {
        foundCount = 1;
        selectedIndex = 1;
    }

    BlockResult result;
    result.success = true;
    result.message = "발견 (" + std::to_string(match.confidence) + ", scale " +
                     std::to_string(match.scale) + ") @ (" + std::to_string(match.center.x) +
                     "," + std::to_string(match.center.y) + ")";
    if (!matchedTemplatePath.empty()) {
        const std::filesystem::path fileName = std::filesystem::path(matchedTemplatePath).filename();
        result.message += " [" + fileName.string() + "]";
    }
    if (foundCount > 1) {
        result.message += " [" + std::to_string(selectedIndex) + "/" + std::to_string(foundCount) + "]";
    }
    result.message += extraMessageSuffix;
    maybeRecordRoiCorrection(ctx,
                             blockRoiCorrection,
                             roiCorrectionExpandPercent,
                             match,
                             searchArea,
                             customRegion,
                             percentRegion);
    ctx.reportProgress(BlockProgressKind::ImageFindSuccess);
    ctx.log(result.message);
    return result;
}

void consumeAllSelections(ExecutionContext& ctx,
                          SearchArea searchArea,
                          const CaptureRegion& customRegion,
                          const PercentRegion& percentRegion,
                          const std::vector<ImageFindSelection>& selections) {
    for (const ImageFindSelection& selection : selections) {
        if (!selection.ok) {
            continue;
        }
        const MatchResult& match = selection.match;
        ctx.consumeMatchRegion(
            ScreenCapture::haystackTopLeftToPhysical(
                searchArea, customRegion, percentRegion, match.location),
            match.matchedSize);
    }
}

} // namespace

bool ImageFindBlock::hasTemplates() const {
    for (const std::string& path : templatePaths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

std::string ImageFindBlock::primaryTemplatePath() const {
    for (const std::string& path : templatePaths) {
        if (!path.empty()) {
            return path;
        }
    }
    return {};
}

std::string ImageFindBlock::summary() const {
    if (searchArea == SearchArea::CustomRegion) {
        const int roiCount = static_cast<int>(customRegionsWindowPercent.size());
        if (roiCount > 1) {
            return "ROI " + std::to_string(roiCount) + "개";
        }
        if (!customRegionsWindowPercent.empty()) {
            const PercentRegion& region = customRegionsWindowPercent.front();
            return "ROI " + std::to_string(static_cast<int>(region.x)) + "%," +
                   std::to_string(static_cast<int>(region.y)) + "%";
        }
    }
    if (searchArea == SearchArea::ScreenPercent) {
        return "ROI " + std::to_string(static_cast<int>(percentRegion.x)) + "%," +
               std::to_string(static_cast<int>(percentRegion.y)) + "%";
    }
    const int templateCount = static_cast<int>(templatePaths.size());
    if (templateCount > 1) {
        const std::string modeLabel =
            templateMatchMode == ImageFindTemplateMatchMode::All ? "모두" : "하나라도";
        return "템플릿 " + std::to_string(templateCount) + "개 (" + modeLabel + ")";
    }
    return "템플릿 매칭";
}

MatchOptions ImageFindBlock::matchOptions() const {
    MatchOptions options;
    options.threshold = threshold;
    options.multiScale = multiScale;
    options.minScale = minScale;
    options.maxScale = maxScale;
    options.templateColorMode = templateColorMode;
    return options;
}

const PreparedTemplate& ImageFindBlock::cachedTemplateFor(const std::string& resolvedPath) const {
    auto it = m_cachedTemplates.find(resolvedPath);
    if (it == m_cachedTemplates.end() || it->second.empty()) {
        PreparedTemplate loaded = ImageMatcher::loadTemplate(resolvedPath);
        m_cachedTemplates[resolvedPath] = std::move(loaded);
        return m_cachedTemplates[resolvedPath];
    }
    return it->second;
}

BlockResult ImageFindBlock::execute(ExecutionContext& ctx) {
    RunRoiOverlayGuard roiOverlayGuard;

    SearchArea runtimeSearchArea = searchArea;
    std::vector<PercentRegion> runtimeWindowPercentRegions = customRegionsWindowPercent;
    PercentRegion runtimePercentRegion = percentRegion;

    if (ctx.shouldUseRoiCorrectionForBlock(roiCorrection) && ctx.runLoopNumber() >= 2) {
        if (const std::optional<PercentRegion> corrected = ctx.correctedRoi(ctx.activeBlockIndex())) {
            runtimeSearchArea = SearchArea::CustomRegion;
            runtimeWindowPercentRegions = { *corrected };
            runtimePercentRegion = {};
        }
    }

    if (runtimeSearchArea == SearchArea::ScreenPercent && isValidWindowPercentRegion(runtimePercentRegion)) {
        runtimeSearchArea = SearchArea::CustomRegion;
        runtimeWindowPercentRegions = { runtimePercentRegion };
    }

    normalizeImageFindSearchArea(runtimeSearchArea, runtimeWindowPercentRegions);

    const int effectiveRoiCorrectionExpandPercent =
        ctx.featureRoiCorrectionGlobal() ? ctx.featureRoiCorrectionExpandPercent()
                                         : roiCorrectionExpandPercent;

    std::vector<std::string> relativePaths;
    std::vector<const PreparedTemplate*> templates;
    relativePaths.reserve(templatePaths.size());
    templates.reserve(templatePaths.size());
    for (const std::string& path : templatePaths) {
        if (path.empty()) {
            continue;
        }
        const std::string resolved = ctx.resolvePath(path);
        const PreparedTemplate& templ = cachedTemplateFor(resolved);
        if (templ.empty()) {
            BlockResult result;
            result.success = false;
            result.message = "템플릿을 찾을 수 없음: " + resolved;
            return result;
        }
        relativePaths.push_back(path);
        templates.push_back(&templ);
    }
    if (relativePaths.empty()) {
        BlockResult result;
        result.success = false;
        result.message = "템플릿이 지정되지 않음";
        return result;
    }

    if (ctx.consumeImageFindPrimedBlockIndex(ctx.activeBlockIndex())) {
        BlockResult result;
        if (!ctx.hasLastMatch()) {
            result.success = false;
            result.message = "트리거 상태가 유효하지 않음";
            return result;
        }
        result.success = true;
        result.message = "트리거 발동";
        ctx.reportProgress(BlockProgressKind::ImageFindSuccess);
        ctx.log(result.message);
        return result;
    }

    const MatchOptions options = matchOptions();
    int missAttempts = 0;
    int pollAttemptCount = 0;
    int64_t matchWorkMs = 0;
    auto lapStart = std::chrono::steady_clock::now();
    const auto accumulateMatchWork = [&]() {
        const auto now = std::chrono::steady_clock::now();
        matchWorkMs += std::max<int64_t>(
            0,
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lapStart).count());
    };
#ifdef _WIN32
    const HWND targetWindow = ctx.targetWindow();
#else
    const HWND targetWindow = nullptr;
#endif

    while (true) {
        ++pollAttemptCount;
        const std::vector<CaptureRegion> pollRegions =
            physicalCustomPollRegions(runtimeSearchArea, runtimeWindowPercentRegions);

        if (pollAttemptCount == 1) {
            const CaptureRegion overlayLegacy =
                pollRegions.empty() ? CaptureRegion{} : pollRegions.front();
            std::vector<CaptureRegion> overlayRegions = pollRegions;
            if (runtimeSearchArea != SearchArea::CustomRegion) {
                overlayRegions.clear();
            }
            WorkflowRoiFlashOverlay::showSearchArea(runtimeSearchArea,
                                                    overlayLegacy,
                                                    runtimePercentRegion,
                                                    overlayRegions);
        }

        ctx.setImageFindPollAttempt(pollAttemptCount);
        lapStart = std::chrono::steady_clock::now();

        WorkflowMatchFeedbackOverlay::hideBeforeCapture();

        if (ctx.shouldStop()) {
            accumulateMatchWork();
            BlockResult result;
            result.success = false;
            result.message = "중지됨";
            result.imageFindMatchDurationMs = matchWorkMs;
            result.imageFindPollAttempts = pollAttemptCount;
            return result;
        }
        if (!ctx.waitWhilePaused()) {
            accumulateMatchWork();
            BlockResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            result.imageFindMatchDurationMs = matchWorkMs;
            result.imageFindPollAttempts = pollAttemptCount;
            return result;
        }

        MatchResult bestMissPeak;
        bool anyValidHaystack = false;
        CaptureRegion missReportRegion = pollRegions.front();

        for (const CaptureRegion& activeRegion : pollRegions) {
            missReportRegion = activeRegion;
            const cv::Mat haystack = ScreenCapture::captureSearchAreaForImageFind(
                runtimeSearchArea, activeRegion, runtimePercentRegion);
            if (haystack.empty() || ScreenCapture::isMostlyBlack(haystack)) {
                continue;
            }
            anyValidHaystack = true;
            const cv::Mat hayGray = ImageMatcher::toGrayscale(haystack);

            if (templateMatchMode == ImageFindTemplateMatchMode::Any) {
                for (size_t index = 0; index < templates.size(); ++index) {
                    const ImageFindSelection selection = trySelectImageFindMatch(
                        haystack, hayGray, *templates[index], options, ctx, runtimeSearchArea, activeRegion, runtimePercentRegion);
                    if (selection.ok) {
                        accumulateMatchWork();
                        BlockResult result = imageFindSuccessResult(ctx,
                                                                  roiCorrection,
                                                                  effectiveRoiCorrectionExpandPercent,
                                                                  haystack,
                                                                  selection,
                                                                  runtimeSearchArea,
                                                                  activeRegion,
                                                                  runtimePercentRegion,
                                                                  targetWindow,
                                                                  threshold,
                                                                  relativePaths[index]);
                        result.imageFindMatchDurationMs = matchWorkMs;
                        result.imageFindPollAttempts = pollAttemptCount;
                        return result;
                    }
                    if (selection.peak.found
                        && (!bestMissPeak.found || selection.peak.confidence > bestMissPeak.confidence)) {
                        bestMissPeak = selection.peak;
                    }
                }
                continue;
            }

            std::vector<ImageFindSelection> selections;
            selections.reserve(templates.size());
            bool allMatched = true;
            for (const PreparedTemplate* templ : templates) {
                const ImageFindSelection selection = trySelectImageFindMatch(
                    haystack, hayGray, *templ, options, ctx, runtimeSearchArea, activeRegion, runtimePercentRegion);
                selections.push_back(selection);
                if (!selection.ok) {
                    allMatched = false;
                    if (selection.peak.found
                        && (!bestMissPeak.found || selection.peak.confidence > bestMissPeak.confidence)) {
                        bestMissPeak = selection.peak;
                    }
                }
            }

            if (allMatched && !selections.empty()) {
                consumeAllSelections(ctx, runtimeSearchArea, activeRegion, runtimePercentRegion, selections);
                std::string extraSuffix;
                if (templates.size() > 1) {
                    std::ostringstream details;
                    details << " (모두 " << templates.size() << "개";
                    for (size_t index = 0; index < selections.size(); ++index) {
                        details << ", " << selections[index].match.confidence;
                    }
                    details << ")";
                    extraSuffix = details.str();
                }
                accumulateMatchWork();
                BlockResult result = imageFindSuccessResult(ctx,
                                                            roiCorrection,
                                                            effectiveRoiCorrectionExpandPercent,
                                                            haystack,
                                                            selections.front(),
                                                            runtimeSearchArea,
                                                            activeRegion,
                                                            runtimePercentRegion,
                                                            targetWindow,
                                                            threshold,
                                                            relativePaths.front(),
                                                            false,
                                                            extraSuffix);
                result.imageFindMatchDurationMs = matchWorkMs;
                result.imageFindPollAttempts = pollAttemptCount;
                return result;
            }
        }

        reportImageFindMiss(ctx,
                            threshold,
                            anyValidHaystack ? bestMissPeak : MatchResult{},
                            runtimeSearchArea,
                            missReportRegion,
                            runtimePercentRegion,
                            targetWindow);
        if (imageFindExceededMissLimit(ctx, missAttempts)) {
            accumulateMatchWork();
            BlockResult result = imageFindDetectionFailureResult(ctx);
            result.imageFindMatchDurationMs = matchWorkMs;
            result.imageFindPollAttempts = pollAttemptCount;
            return result;
        }
        accumulateMatchWork();
        if (!sleepUnlessStopped(ctx, pollIntervalMs)) {
            BlockResult result;
            result.success = false;
            result.message = "사용자에 의해 중지됨";
            result.imageFindMatchDurationMs = matchWorkMs;
            result.imageFindPollAttempts = pollAttemptCount;
            return result;
        }
    }
}

nlohmann::json ImageFindBlock::toJson() const {
    nlohmann::json templatesJson = nlohmann::json::array();
    for (const std::string& path : templatePaths) {
        if (!path.empty()) {
            templatesJson.push_back(path);
        }
    }

    nlohmann::json json{
        {"type", "ImageFind"},
        {"templates", templatesJson},
        {"templateMatchMode", templateMatchModeToString(templateMatchMode)},
        {"threshold", threshold},
        {"pollIntervalMs", pollIntervalMs},
        {"searchArea", searchAreaToString(searchArea)},
        {"customRegion", captureRegionToJson(customRegion)},
        {"percentRegion",
         {{"x", percentRegion.x},
          {"y", percentRegion.y},
          {"width", percentRegion.width},
          {"height", percentRegion.height}}}};
    std::vector<PercentRegion> percentToSave = customRegionsWindowPercent;
    pruneInvalidWindowPercentRegions(percentToSave);
    if (!percentToSave.empty()) {
        nlohmann::json regionsJson = nlohmann::json::array();
        for (const PercentRegion& region : percentToSave) {
            regionsJson.push_back(percentRegionToJson(region));
        }
        json["customRegions"] = std::move(regionsJson);
        json["customRegionsAnchoredToTargetWindow"] = true;
    }
    if (multiScale) {
        json["multiScale"] = true;
    }
    if (std::abs(minScale - 0.9) > 1e-9) {
        json["minScale"] = minScale;
    }
    if (std::abs(maxScale - 1.1) > 1e-9) {
        json["maxScale"] = maxScale;
    }
    if (roiCorrection) {
        json["roiCorrection"] = true;
    }
    if (roiCorrectionExpandPercent != kDefaultRoiCorrectionExpandPercent) {
        json["roiCorrectionExpandPercent"] = roiCorrectionExpandPercent;
    }
    if (returnToPreviousImageFindOnFailure) {
        json["returnToPreviousImageFindOnFailure"] = true;
    }
    if (retryAfterNextActionOnFailure) {
        json["retryAfterNextActionOnFailure"] = true;
    }
    if (templateColorMode != TemplateColorMode::Auto) {
        json["templateColorMode"] = templateColorModeToString(templateColorMode);
    }
    return json;
}

std::unique_ptr<Block> ImageFindBlock::clone() const {
    auto copy = std::make_unique<ImageFindBlock>(*this);
    return copy;
}

std::unique_ptr<ImageFindBlock> ImageFindBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<ImageFindBlock>();
    block->templatePaths.clear();
    if (json.contains("templates") && json["templates"].is_array()) {
        for (const auto& item : json["templates"]) {
            if (item.is_string()) {
                block->templatePaths.push_back(item.get<std::string>());
            }
        }
    }
    pruneEmptyTemplatePaths(block->templatePaths);
    block->templateMatchMode =
        templateMatchModeFromString(json.value("templateMatchMode", "Any"));
    block->threshold = json.value("threshold", 0.85);
    block->multiScale = json.value("multiScale", false);
    block->minScale = json.value("minScale", 0.9);
    block->maxScale = json.value("maxScale", 1.1);
    block->pollIntervalMs = snapImageFindPollIntervalMs(
        json.value("pollIntervalMs", kDefaultImageFindPollIntervalMs));
    block->searchArea = searchAreaFromString(json.value("searchArea", "TargetWindow"));
    block->customRegions.clear();
    block->customRegionsWindowPercent.clear();
    block->customRegion = {};
    const bool legacyAnchored = json.value("customRegionsAnchoredToTargetWindow", false);
    if (json.contains("customRegions") && json["customRegions"].is_array()) {
        for (const auto& region : json["customRegions"]) {
            if (!region.is_object()) {
                continue;
            }
            if (legacyAnchored || anchoredJsonLooksLikePixelOffset(region)) {
                if (anchoredJsonLooksLikePixelOffset(region)) {
                    block->customRegionsWindowPercent.push_back(
                        pixelOffsetToWindowPercent(captureRegionFromJson(region)));
                } else {
                    block->customRegionsWindowPercent.push_back(percentRegionFromJson(region));
                }
            } else {
                block->customRegionsWindowPercent.push_back(
                    ScreenCapture::storeWindowPercentFromPhysical(captureRegionFromJson(region)));
            }
        }
    }
    if (block->customRegionsWindowPercent.empty() && json.contains("customRegion")) {
        const CaptureRegion legacy = captureRegionFromJson(json["customRegion"]);
        if (isValidCustomRegion(legacy)) {
            block->customRegionsWindowPercent.push_back(
                ScreenCapture::storeWindowPercentFromPhysical(legacy));
        }
    }
    pruneInvalidWindowPercentRegions(block->customRegionsWindowPercent);
    if (json.contains("percentRegion")) {
        const auto& region = json["percentRegion"];
        block->percentRegion.x = region.value("x", 0.0);
        block->percentRegion.y = region.value("y", 0.0);
        block->percentRegion.width = region.value("width", 100.0);
        block->percentRegion.height = region.value("height", 100.0);
    }
    if (block->searchArea == SearchArea::ScreenPercent
        && isValidWindowPercentRegion(block->percentRegion)) {
        block->customRegionsWindowPercent.push_back(block->percentRegion);
        block->searchArea = SearchArea::CustomRegion;
    }
    block->customRegionsAnchoredToTargetWindow = true;
    if (!block->customRegionsWindowPercent.empty()) {
        block->searchArea = SearchArea::CustomRegion;
    } else {
        normalizeImageFindSearchArea(block->searchArea, block->customRegionsWindowPercent);
    }
    block->roiCorrection = json.value("roiCorrection", false);
    block->roiCorrectionExpandPercent = snapRoiCorrectionExpandPercent(
        json.value("roiCorrectionExpandPercent", kDefaultRoiCorrectionExpandPercent));
    block->returnToPreviousImageFindOnFailure = json.value("returnToPreviousImageFindOnFailure", false);
    block->retryAfterNextActionOnFailure = json.value("retryAfterNextActionOnFailure", false);
    block->templateColorMode =
        templateColorModeFromString(json.value("templateColorMode", "Auto"));
    return block;
}

ImageFindMatchTestResult ImageFindBlock::testMatch(SearchArea searchArea,
                                                   const PercentRegion& percentRegion,
                                                   const std::string& templatePath,
                                                   const MatchOptions& options,
                                                   const std::string& projectDirectory,
                                                   const std::vector<PercentRegion>& customRegionsWindowPercent) {
    ImageFindMatchTestResult result;
    const auto lapStart = std::chrono::steady_clock::now();
    const auto recordMatchDuration = [&]() {
        result.matchDurationMs = std::max<int64_t>(
            0,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                  - lapStart)
                .count());
    };

    SearchArea resolvedSearchArea = searchArea;
    std::vector<PercentRegion> resolvedWindowPercent = customRegionsWindowPercent;
    PercentRegion resolvedPercentRegion = percentRegion;
    if (resolvedSearchArea == SearchArea::ScreenPercent
        && isValidWindowPercentRegion(resolvedPercentRegion)) {
        resolvedSearchArea = SearchArea::CustomRegion;
        resolvedWindowPercent = { resolvedPercentRegion };
    }
    normalizeImageFindSearchArea(resolvedSearchArea, resolvedWindowPercent);
    const std::vector<CaptureRegion> pollRegions =
        physicalCustomPollRegions(resolvedSearchArea, resolvedWindowPercent);
    CaptureRegion resolvedCustomRegion =
        pollRegions.empty() ? CaptureRegion{} : pollRegions.front();

#ifdef _WIN32
    if (resolvedSearchArea == SearchArea::TargetWindow && !ScreenCapture::findTargetWindow()
        && !ScreenCapture::allowsRunWithoutTargetWindow()) {
        result.errorMessage = "대상 창을 찾을 수 없습니다. 먼저 '창 지정'을 사용하세요.";
        return result;
    }
    ScreenCapture::activateTargetWindow();
#endif

    const std::string resolved = resolveTemplatePath(templatePath, projectDirectory);
    const PreparedTemplate templ = ImageMatcher::loadTemplate(resolved);
    if (templ.empty()) {
        result.errorMessage = "템플릿을 찾을 수 없음: " + resolved;
        return result;
    }
    result.needle = templ.bgr;

    result.haystack = ScreenCapture::captureSearchAreaForImageFind(
        resolvedSearchArea, resolvedCustomRegion, percentRegion);
    if (result.haystack.empty()) {
        if (resolvedSearchArea == SearchArea::TargetWindow) {
            result.errorMessage = "대상 창을 캡처하지 못했습니다. 먼저 '창 지정'을 사용하세요.";
        } else if (resolvedSearchArea == SearchArea::CustomRegion) {
            result.errorMessage = "사용자 영역을 캡처하지 못했습니다. X/Y/W/H 값을 확인하세요.";
        } else if (resolvedSearchArea == SearchArea::ScreenPercent) {
            result.errorMessage = "화면 비율 영역을 캡처하지 못했습니다. % 값을 확인하세요.";
        } else {
            result.errorMessage = "화면을 캡처하지 못했습니다.";
        }
        return result;
    }

    if (ScreenCapture::isMostlyBlack(result.haystack)) {
        result.errorMessage = "캡처된 영역이 거의 검은색입니다. 창 모드 또는 보더리스 창 모드를 확인하세요.";
        return result;
    }

    MatchOptions matchOptions = options;
    const cv::Mat hayGray = ImageMatcher::toGrayscale(result.haystack);
    MatchResult peak = ImageMatcher::findPeakMatchGray(hayGray, templ, matchOptions);
    result.matches = ImageMatcher::findAllTemplatesGray(hayGray, templ, matchOptions, true);
    const bool requireGrayscaleHaystack =
        ImageMatcher::requiresGrayscaleHaystackRegion(matchOptions.templateColorMode, templ);
    applyGrayscaleHaystackFilter(result.haystack, requireGrayscaleHaystack, result.matches, peak);
    if (!result.matches.empty()) {
        result.match = result.matches.front();
    } else {
        result.match = peak;
    }
    result.captureOk = true;
    recordMatchDuration();
    return result;
}

ImageFindMatchTestResult ImageFindBlock::testMatchTemplates(SearchArea searchArea,
                                                            const PercentRegion& percentRegion,
                                                            const std::vector<std::string>& templatePaths,
                                                            const MatchOptions& options,
                                                            const std::string& projectDirectory,
                                                            const std::vector<PercentRegion>& customRegionsWindowPercent) {
    ImageFindMatchTestResult result;
    const auto lapStart = std::chrono::steady_clock::now();
    const auto recordMatchDuration = [&]() {
        result.matchDurationMs = std::max<int64_t>(
            0,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                  - lapStart)
                .count());
    };

    std::vector<std::string> resolvedPaths;
    resolvedPaths.reserve(templatePaths.size());
    for (const std::string& path : templatePaths) {
        if (path.empty()) {
            continue;
        }
        resolvedPaths.push_back(resolveTemplatePath(path, projectDirectory));
    }
    if (resolvedPaths.empty()) {
        result.errorMessage = "템플릿을 먼저 지정하세요.";
        return result;
    }

    SearchArea resolvedSearchArea = searchArea;
    std::vector<PercentRegion> resolvedWindowPercent = customRegionsWindowPercent;
    PercentRegion resolvedPercentRegion = percentRegion;
    if (resolvedSearchArea == SearchArea::ScreenPercent
        && isValidWindowPercentRegion(resolvedPercentRegion)) {
        resolvedSearchArea = SearchArea::CustomRegion;
        resolvedWindowPercent = { resolvedPercentRegion };
    }
    normalizeImageFindSearchArea(resolvedSearchArea, resolvedWindowPercent);

    std::vector<CaptureRegion> pollRegions =
        physicalCustomPollRegions(resolvedSearchArea, resolvedWindowPercent);

#ifdef _WIN32
    if (resolvedSearchArea == SearchArea::TargetWindow && !ScreenCapture::findTargetWindow()
        && !ScreenCapture::allowsRunWithoutTargetWindow()) {
        result.errorMessage = "대상 창을 찾을 수 없습니다. 먼저 '창 지정'을 사용하세요.";
        return result;
    }
    ScreenCapture::activateTargetWindow();
#endif

    MatchOptions matchOptions = options;
    MatchResult bestPeak;
    bool capturedAnyHaystack = false;
    const bool multiCustomRoi =
        resolvedSearchArea == SearchArea::CustomRegion && pollRegions.size() > 1;

    for (const CaptureRegion& activeRegion : pollRegions) {
        cv::Mat haystack = ScreenCapture::captureSearchAreaForImageFind(
            resolvedSearchArea, activeRegion, percentRegion);
        if (haystack.empty() || ScreenCapture::isMostlyBlack(haystack)) {
            continue;
        }
        capturedAnyHaystack = true;
        if (result.haystack.empty()) {
            result.haystack = haystack;
        }

        const cv::Mat hayGray = ImageMatcher::toGrayscale(haystack);
        std::vector<MatchResult> regionMatches;
        for (const std::string& resolved : resolvedPaths) {
            const PreparedTemplate templ = ImageMatcher::loadTemplate(resolved);
            if (templ.empty()) {
                result.errorMessage = "템플릿을 찾을 수 없음: " + resolved;
                return result;
            }
            if (result.needle.empty()) {
                result.needle = templ.bgr;
            }
            std::vector<MatchResult> templateMatches =
                ImageMatcher::findAllTemplatesGray(hayGray, templ, matchOptions, true);
            MatchResult peak = ImageMatcher::findPeakMatchGray(hayGray, templ, matchOptions);
            const bool requireGrayscaleHaystack =
                ImageMatcher::requiresGrayscaleHaystackRegion(matchOptions.templateColorMode, templ);
            applyGrayscaleHaystackFilter(haystack, requireGrayscaleHaystack, templateMatches, peak);
            if (multiCustomRoi) {
                regionMatches.insert(regionMatches.end(), templateMatches.begin(), templateMatches.end());
            } else {
                result.matches.insert(result.matches.end(), templateMatches.begin(), templateMatches.end());
            }

            if (peak.found && (!bestPeak.found || peak.confidence > bestPeak.confidence)) {
                bestPeak = peak;
            }
        }
        if (multiCustomRoi && !regionMatches.empty()) {
            result.matchesPerCustomRegion.emplace_back(activeRegion, std::move(regionMatches));
        }
    }

    if (!capturedAnyHaystack) {
        if (resolvedSearchArea == SearchArea::TargetWindow) {
            result.errorMessage = "대상 창을 캡처하지 못했습니다. 먼저 '창 지정'을 사용하세요.";
        } else if (resolvedSearchArea == SearchArea::CustomRegion) {
            result.errorMessage = "사용자 영역을 캡처하지 못했습니다. ROI를 확인하세요.";
        } else if (resolvedSearchArea == SearchArea::ScreenPercent) {
            result.errorMessage = "화면 비율 영역을 캡처하지 못했습니다. % 값을 확인하세요.";
        } else {
            result.errorMessage = "화면을 캡처하지 못했습니다.";
        }
        return result;
    }

    if (!result.matches.empty()) {
        result.match = result.matches.front();
    } else if (!result.matchesPerCustomRegion.empty()) {
        for (const auto& batch : result.matchesPerCustomRegion) {
            if (!batch.second.empty()) {
                result.match = batch.second.front();
                break;
            }
        }
    } else {
        result.match = bestPeak;
    }
    result.captureOk = true;
    recordMatchDuration();
    return result;
}
