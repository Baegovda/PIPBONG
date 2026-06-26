#include "core/workflow/ExecutionContext.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/blocks/IfBlock.h"

#include <chrono>
#include <filesystem>
#include <thread>

namespace {

bool regionsOverlap(const cv::Point& topLeftA, const cv::Size& sizeA, const cv::Point& topLeftB,
                    const cv::Size& sizeB) {
    if (sizeA.width <= 0 || sizeA.height <= 0 || sizeB.width <= 0 || sizeB.height <= 0) {
        return false;
    }
    const cv::Rect rectA(topLeftA.x, topLeftA.y, sizeA.width, sizeA.height);
    const cv::Rect rectB(topLeftB.x, topLeftB.y, sizeB.width, sizeB.height);
    return (rectA & rectB).area() > 0;
}

} // namespace

void ExecutionContext::setLogCallback(LogCallback callback) {
    m_logCallback = std::move(callback);
}

void ExecutionContext::log(const std::string& message) const {
    if (m_logCallback) {
        m_logCallback(message);
    }
}

void ExecutionContext::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void ExecutionContext::clearProgressCallback() {
    m_progressCallback = nullptr;
}

void ExecutionContext::reportProgress(BlockProgressKind kind) const {
    if (m_progressCallback) {
        m_progressCallback(kind);
    }
}

void ExecutionContext::requestStop() {
    m_stopRequested.store(true);
}

bool ExecutionContext::shouldStop() const {
    return m_stopRequested.load();
}

void ExecutionContext::resetStop() {
    m_stopRequested.store(false);
    m_paused.store(false);
    m_detectionFailedThisRun = false;
    m_ifNestingDepth = 0;
    clearLastMatch();
    clearLastMatchAttempt();
    clearConsumedMatchRegions();
}

void ExecutionContext::setPaused(bool paused) {
    m_paused.store(paused);
}

bool ExecutionContext::isPaused() const {
    return m_paused.load();
}

void ExecutionContext::togglePaused() {
    m_paused.store(!m_paused.load());
}

bool ExecutionContext::interruptibleSleepMs(int delayMs) {
    const auto step = std::chrono::milliseconds(50);
    auto remaining = std::chrono::milliseconds(delayMs);
    while (remaining.count() > 0) {
        if (shouldStop()) {
            return false;
        }
        if (!waitWhilePaused()) {
            return false;
        }
        const auto sleepFor = remaining > step ? step : remaining;
        std::this_thread::sleep_for(sleepFor);
        remaining -= sleepFor;
    }
    return !shouldStop();
}

bool ExecutionContext::waitWhilePaused() {
    const auto step = std::chrono::milliseconds(50);
    while (isPaused()) {
        if (shouldStop()) {
            return false;
        }
        std::this_thread::sleep_for(step);
    }
    return !shouldStop();
}

void ExecutionContext::setImageFindMaxMissAttempts(int attempts) {
    m_imageFindMaxMissAttempts = attempts < 0 ? 0 : attempts;
}

int ExecutionContext::imageFindMaxMissAttempts() const {
    return m_imageFindMaxMissAttempts;
}

void ExecutionContext::markDetectionFailure() {
    m_detectionFailedThisRun = true;
}

void ExecutionContext::clearDetectionFailedFlag() {
    m_detectionFailedThisRun = false;
}

bool ExecutionContext::detectionFailedThisRun() const {
    return m_detectionFailedThisRun;
}

bool ExecutionContext::enterIfScope() {
    if (m_ifNestingDepth >= IfBlock::kMaxNestingDepth) {
        return false;
    }
    ++m_ifNestingDepth;
    return true;
}

void ExecutionContext::leaveIfScope() {
    if (m_ifNestingDepth > 0) {
        --m_ifNestingDepth;
    }
}

int ExecutionContext::ifNestingDepth() const {
    return m_ifNestingDepth;
}

bool ExecutionContext::hasLastMatch() const {
    return m_hasLastMatch;
}

cv::Point ExecutionContext::lastMatchPoint() const {
    return m_lastMatchPoint;
}

bool ExecutionContext::hasLastMatchScreenPoint() const {
    return m_hasLastMatchScreenPoint;
}

cv::Point ExecutionContext::lastMatchScreenPoint() const {
    return m_lastMatchScreenPoint;
}

double ExecutionContext::lastMatchConfidence() const {
    return m_lastMatchConfidence;
}

double ExecutionContext::lastMatchThreshold() const {
    return m_lastMatchThreshold;
}

bool ExecutionContext::hasLastMatchPreview() const {
    return !m_lastMatchPreview.empty();
}

cv::Mat ExecutionContext::lastMatchPreview() const {
    return m_lastMatchPreview;
}

void ExecutionContext::setLastMatch(const cv::Point& point,
                                      double confidence,
                                      const cv::Mat& previewBgr,
                                      double matchThreshold) {
    m_hasLastMatch = true;
    m_lastMatchPoint = point;
    m_lastMatchConfidence = confidence;
    m_lastMatchThreshold = matchThreshold;
    m_lastMatchPreview = previewBgr.clone();
}

void ExecutionContext::setLastMatchScreenPoint(const cv::Point& point) {
    m_hasLastMatchScreenPoint = true;
    m_lastMatchScreenPoint = point;
}

void ExecutionContext::clearLastMatch() {
    m_hasLastMatch = false;
    m_lastMatchPoint = {};
    m_hasLastMatchScreenPoint = false;
    m_lastMatchScreenPoint = {};
    m_lastMatchConfidence = 0.0;
    m_lastMatchThreshold = 0.0;
    m_lastMatchPreview.release();
}

void ExecutionContext::setLastMatchAttempt(double matchThreshold,
                                           double peakConfidence,
                                           const cv::Point& clientPoint,
                                           bool hasClientPoint) {
    m_hasLastMatchAttempt = true;
    m_lastMatchAttemptThreshold = matchThreshold;
    m_lastMatchAttemptConfidence = peakConfidence;
    m_hasLastMatchAttemptPoint = hasClientPoint;
    m_lastMatchAttemptClientPoint = clientPoint;
}

bool ExecutionContext::hasLastMatchAttempt() const {
    return m_hasLastMatchAttempt;
}

double ExecutionContext::lastMatchAttemptConfidence() const {
    return m_lastMatchAttemptConfidence;
}

double ExecutionContext::lastMatchAttemptThreshold() const {
    return m_lastMatchAttemptThreshold;
}

bool ExecutionContext::hasLastMatchAttemptPoint() const {
    return m_hasLastMatchAttemptPoint;
}

cv::Point ExecutionContext::lastMatchAttemptClientPoint() const {
    return m_lastMatchAttemptClientPoint;
}

void ExecutionContext::clearLastMatchAttempt() {
    m_hasLastMatchAttempt = false;
    m_lastMatchAttemptConfidence = 0.0;
    m_lastMatchAttemptThreshold = 0.0;
    m_hasLastMatchAttemptPoint = false;
    m_lastMatchAttemptClientPoint = {};
}

bool ExecutionContext::isMatchRegionConsumed(const cv::Point& topLeft, const cv::Size& size) const {
    for (const ConsumedMatchRegion& consumed : m_consumedMatchRegions) {
        if (regionsOverlap(topLeft, size, consumed.topLeft, consumed.size)) {
            return true;
        }
    }
    return false;
}

bool ExecutionContext::hasConsumedMatchRegions() const {
    return !m_consumedMatchRegions.empty();
}

void ExecutionContext::consumeMatchRegion(const cv::Point& topLeft, const cv::Size& size) {
    if (size.width <= 0 || size.height <= 0) {
        return;
    }
    m_consumedMatchRegions.push_back({topLeft, size});
}

void ExecutionContext::clearConsumedMatchRegions() {
    m_consumedMatchRegions.clear();
}

void ExecutionContext::setTargetWindowTitle(const std::wstring& title) {
    m_targetWindowTitle = title;
    ScreenCapture::setTargetWindowTitle(title);
}

std::wstring ExecutionContext::targetWindowTitle() const {
    return m_targetWindowTitle;
}

#ifdef _WIN32
HWND ExecutionContext::targetWindow() const {
    HWND hwnd = ScreenCapture::targetWindow();
    if (hwnd && IsWindow(hwnd)) {
        return hwnd;
    }
    return ScreenCapture::findTargetWindow();
}
#endif

std::string ExecutionContext::resolvePath(const std::string& relativePath) const {
    if (relativePath.empty()) {
        return relativePath;
    }
    std::filesystem::path path(relativePath);
    if (path.is_absolute()) {
        return relativePath;
    }
    if (m_projectDirectory.empty()) {
        return relativePath;
    }
    return (std::filesystem::path(m_projectDirectory) / path).string();
}

void ExecutionContext::setProjectDirectory(const std::string& path) {
    m_projectDirectory = path;
}

std::string ExecutionContext::projectDirectory() const {
    return m_projectDirectory;
}

#ifdef _WIN32
void ExecutionContext::beginRunKeyboardSessionIfNeeded() {
    if (m_runKeyboardSessionActive) {
        return;
    }
    m_runKeyboardSessionStart = InputSimulator::captureSessionModifierSnapshot();
    m_runKeyboardSessionActive = true;
}

void ExecutionContext::noteSyntheticKeyDown(int virtualKey) {
    m_sbmHeldVirtualKeys.insert(virtualKey);
}

void ExecutionContext::noteSyntheticKeyUp(int virtualKey) {
    m_sbmHeldVirtualKeys.erase(virtualKey);
}

void ExecutionContext::restoreRunKeyboard() {
    if (!m_runKeyboardSessionActive) {
        return;
    }
    InputSimulator::restoreTrackedKeyboard(m_sbmHeldVirtualKeys, m_runKeyboardSessionStart);
}

void ExecutionContext::endRunKeyboardSession() {
    restoreRunKeyboard();
    m_runKeyboardSessionActive = false;
    m_sbmHeldVirtualKeys.clear();
}
#endif
