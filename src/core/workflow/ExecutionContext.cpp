#include "core/workflow/ExecutionContext.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include <algorithm>
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
    if (m_suppressRepeatUi || !m_logCallback) {
        return;
    }
    m_logCallback(message);
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

void ExecutionContext::setPointerFeedbackCallback(PointerFeedbackCallback callback) {
    m_pointerFeedbackCallback = std::move(callback);
}

void ExecutionContext::clearPointerFeedbackCallback() {
    m_pointerFeedbackCallback = nullptr;
}

void ExecutionContext::setPointerVisualFeedback(bool enabled) {
    m_pointerVisualFeedback = enabled;
}

bool ExecutionContext::pointerVisualFeedback() const {
    return m_pointerVisualFeedback;
}

void ExecutionContext::reportPointerFeedback(int clientX, int clientY) const {
    if (m_suppressRepeatUi || !m_pointerVisualFeedback || !m_pointerFeedbackCallback) {
        return;
    }
    m_pointerFeedbackCallback(clientX, clientY);
}

void ExecutionContext::setSuppressRepeatUi(bool suppress) {
    m_suppressRepeatUi = suppress;
}

bool ExecutionContext::suppressRepeatUi() const {
    return m_suppressRepeatUi;
}

void ExecutionContext::setWorkerFastRepeatCallbacks(WorkerFastRepeatCallbacks callbacks) {
    m_workerFastRepeat = std::move(callbacks);
}

void ExecutionContext::clearWorkerFastRepeatCallbacks() {
    m_workerFastRepeat.reset();
}

bool ExecutionContext::hasWorkerFastRepeat() const {
    return m_workerFastRepeat.has_value() && static_cast<bool>(m_workerFastRepeat->shouldContinue);
}

void ExecutionContext::notifyWorkerFastRepeatIteration(bool lastSuccess,
                                                       std::int64_t elapsedMs,
                                                       const std::string& message) const {
    if (m_workerFastRepeat && m_workerFastRepeat->onIterationComplete) {
        m_workerFastRepeat->onIterationComplete(lastSuccess, elapsedMs, message);
    }
}

bool ExecutionContext::shouldContinueWorkerFastRepeat(bool lastSuccess) const {
    if (shouldStop() || !hasWorkerFastRepeat()) {
        return false;
    }
    return m_workerFastRepeat->shouldContinue(lastSuccess, m_detectionFailedThisRun);
}

int ExecutionContext::workerFastRepeatDelayMs() const {
    if (!m_workerFastRepeat || !m_workerFastRepeat->delayMs) {
        return 0;
    }
    return std::max(0, m_workerFastRepeat->delayMs());
}

void ExecutionContext::prepareNextWorkerRepeatIteration() {
    resetStop();
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
    m_nestingDepth = 0;
    // Trigger action reuses the monitor's last match via imageFindPrimedBlockIndex.
    // resetStop runs again in WorkflowEngine before blocks execute — do not wipe that handoff.
    if (m_imageFindPrimedBlockIndex < 0) {
        clearLastMatch();
        clearLastMatchAttempt();
        clearConsumedMatchRegions();
    }
}

void ExecutionContext::setPaused(bool paused) {
    const bool wasPaused = m_paused.load();
    if (!wasPaused && paused) {
        restoreRunHeldInput();
    }
    m_paused.store(paused);
}

bool ExecutionContext::isPaused() const {
    return m_paused.load();
}

void ExecutionContext::togglePaused() {
    if (!m_paused.load()) {
        restoreRunHeldInput();
    }
    m_paused.store(!m_paused.load());
}

bool ExecutionContext::interruptibleSleepMs(int delayMs) {
    const auto step = std::chrono::milliseconds(10);
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

void ExecutionContext::setScopedTargetPollGate(ScopedTargetPollGate gate) {
    m_scopedTargetPollGate = std::move(gate);
}

void ExecutionContext::clearScopedTargetPollGate() {
    m_scopedTargetPollGate = nullptr;
}

bool ExecutionContext::scopedTargetPollAllowed() const {
    return !m_scopedTargetPollGate || m_scopedTargetPollGate();
}

void ExecutionContext::setTriggerMonitorYieldGate(ScopedTargetPollGate gate) {
    m_triggerMonitorYieldGate = std::move(gate);
}

void ExecutionContext::clearTriggerMonitorYieldGate() {
    m_triggerMonitorYieldGate = nullptr;
}

bool ExecutionContext::triggerMonitorCaptureAllowed() const {
    return !m_triggerMonitorYieldGate || m_triggerMonitorYieldGate();
}

void ExecutionContext::setImageFindMaxMissAttempts(int attempts) {
    m_imageFindMaxMissAttempts = attempts < 0 ? 0 : attempts;
}

int ExecutionContext::imageFindMaxMissAttempts() const {
    return m_imageFindMaxMissAttempts;
}

void ExecutionContext::setImageFindPollAttempt(int attempt) {
    m_imageFindPollAttempt = attempt < 0 ? 0 : attempt;
}

int ExecutionContext::imageFindPollAttempt() const {
    return m_imageFindPollAttempt;
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

bool ExecutionContext::imageFindDeferRetryUsed(int blockIndex) const {
    return m_imageFindDeferRetryUsedBlockIndexes.count(blockIndex) > 0;
}

void ExecutionContext::markImageFindDeferRetryUsed(int blockIndex) {
    if (blockIndex >= 0) {
        m_imageFindDeferRetryUsedBlockIndexes.insert(blockIndex);
    }
}

void ExecutionContext::clearImageFindDeferRetryUsed(int blockIndex) {
    if (blockIndex >= 0) {
        m_imageFindDeferRetryUsedBlockIndexes.erase(blockIndex);
    }
}

void ExecutionContext::incrementImageFindReturnToPreviousCount(int blockIndex) {
    if (blockIndex >= 0) {
        ++m_imageFindReturnToPreviousCounts[blockIndex];
    }
}

void ExecutionContext::incrementImageFindRetryAfterNextCount(int blockIndex) {
    if (blockIndex >= 0) {
        ++m_imageFindRetryAfterNextCounts[blockIndex];
    }
}

int ExecutionContext::imageFindReturnToPreviousCount(int blockIndex) const {
    const auto it = m_imageFindReturnToPreviousCounts.find(blockIndex);
    return it == m_imageFindReturnToPreviousCounts.end() ? 0 : it->second;
}

int ExecutionContext::imageFindRetryAfterNextCount(int blockIndex) const {
    const auto it = m_imageFindRetryAfterNextCounts.find(blockIndex);
    return it == m_imageFindRetryAfterNextCounts.end() ? 0 : it->second;
}

bool ExecutionContext::enterNestingScope() {
    if (m_nestingDepth >= kMaxWorkflowNestingDepth) {
        return false;
    }
    ++m_nestingDepth;
    return true;
}

void ExecutionContext::leaveNestingScope() {
    if (m_nestingDepth > 0) {
        --m_nestingDepth;
    }
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

void ExecutionContext::setTriggerMonitorBlockIndex(int blockIndex) {
    m_triggerMonitorBlockIndex = blockIndex;
}

int ExecutionContext::triggerMonitorBlockIndex() const {
    return m_triggerMonitorBlockIndex;
}

void ExecutionContext::setImageFindPrimedBlockIndex(int blockIndex) {
    m_imageFindPrimedBlockIndex = blockIndex;
}

bool ExecutionContext::consumeImageFindPrimedBlockIndex(int blockIndex) {
    if (m_imageFindPrimedBlockIndex < 0 || m_imageFindPrimedBlockIndex != blockIndex) {
        return false;
    }
    m_imageFindPrimedBlockIndex = -1;
    return true;
}

void ExecutionContext::setRoiCorrectionSession(bool eligible, bool featureGlobal) {
    m_roiCorrectionSessionEligible = eligible;
    m_featureRoiCorrectionGlobal = featureGlobal;
}

bool ExecutionContext::roiCorrectionSessionEligible() const {
    return m_roiCorrectionSessionEligible;
}

bool ExecutionContext::featureRoiCorrectionGlobal() const {
    return m_featureRoiCorrectionGlobal;
}

void ExecutionContext::setFeatureRoiCorrectionExpandPercent(int percent) {
    m_featureRoiCorrectionExpandPercent = snapRoiCorrectionExpandPercent(percent);
}

int ExecutionContext::featureRoiCorrectionExpandPercent() const {
    return m_featureRoiCorrectionExpandPercent;
}

bool ExecutionContext::shouldUseRoiCorrectionForBlock(bool blockRoiCorrection) const {
    if (!m_roiCorrectionSessionEligible) {
        return false;
    }
    if (m_featureRoiCorrectionGlobal) {
        return true;
    }
    return blockRoiCorrection;
}

void ExecutionContext::setRunLoopNumber(int loopNumber) {
    m_runLoopNumber = loopNumber < 1 ? 1 : loopNumber;
}

int ExecutionContext::runLoopNumber() const {
    return m_runLoopNumber;
}

void ExecutionContext::setActiveBlockIndex(int blockIndex) {
    m_activeBlockIndex = blockIndex;
}

int ExecutionContext::activeBlockIndex() const {
    return m_activeBlockIndex;
}

void ExecutionContext::setCorrectedRoi(int blockIndex, const PercentRegion& region) {
    if (blockIndex < 0) {
        return;
    }
    m_correctedRoisByBlockIndex[blockIndex] = region;
}

std::optional<PercentRegion> ExecutionContext::correctedRoi(int blockIndex) const {
    const auto it = m_correctedRoisByBlockIndex.find(blockIndex);
    if (it == m_correctedRoisByBlockIndex.end()) {
        return std::nullopt;
    }
    return it->second;
}

void ExecutionContext::clearCorrectedRois() {
    m_correctedRoisByBlockIndex.clear();
}

bool ExecutionContext::shouldRememberPositionsForBlock(bool blockEnabled) const {
    return m_roiCorrectionSessionEligible && blockEnabled;
}

bool ExecutionContext::hasRememberedPositions(int blockIndex) const {
    const auto it = m_rememberedPositionsByBlockIndex.find(blockIndex);
    return it != m_rememberedPositionsByBlockIndex.end() && !it->second.hits.empty();
}

int ExecutionContext::rememberedPositionCount(int blockIndex) const {
    const auto it = m_rememberedPositionsByBlockIndex.find(blockIndex);
    if (it == m_rememberedPositionsByBlockIndex.end()) {
        return 0;
    }
    return static_cast<int>(it->second.hits.size());
}

void ExecutionContext::setRememberedPositions(int blockIndex,
                                            std::vector<RememberedImageFindHit> hits) {
    if (blockIndex < 0) {
        return;
    }
    RememberedImageFindPositions positions;
    positions.hits = std::move(hits);
    m_rememberedPositionsByBlockIndex[blockIndex] = std::move(positions);
}

std::optional<ExecutionContext::RememberedImageFindHit>
ExecutionContext::rememberedPositionAt(int blockIndex, int zeroBasedIndex) const {
    const auto it = m_rememberedPositionsByBlockIndex.find(blockIndex);
    if (it == m_rememberedPositionsByBlockIndex.end()) {
        return std::nullopt;
    }
    const RememberedImageFindPositions& positions = it->second;
    if (zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int>(positions.hits.size())) {
        return std::nullopt;
    }
    return positions.hits[static_cast<size_t>(zeroBasedIndex)];
}

void ExecutionContext::clearRememberedPositions() {
    m_rememberedPositionsByBlockIndex.clear();
}

void ExecutionContext::setTargetWindowTitle(const std::wstring& title) {
    m_targetWindowTitle = title;
    m_cachedTargetWindow = nullptr;
}

void ExecutionContext::setTargetWindowTitleForWorker(const std::wstring& title) {
    setTargetWindowTitle(title);
}

std::wstring ExecutionContext::targetWindowTitle() const {
    return m_targetWindowTitle;
}

#ifdef _WIN32
HWND ExecutionContext::targetWindow() const {
    if (m_cachedTargetWindow && IsWindow(m_cachedTargetWindow)) {
        return m_cachedTargetWindow;
    }
    refreshTargetWindowHandle();
    return m_cachedTargetWindow;
}

void ExecutionContext::refreshTargetWindowHandle() const {
    m_cachedTargetWindow = nullptr;
    ScreenCapture::invalidateTargetWindowCache();
    if (m_targetWindowTitle.empty()) {
        return;
    }
    HWND hwnd = ScreenCapture::findVisibleWindowMatchingTitle(m_targetWindowTitle);
    if (hwnd && IsWindow(hwnd)) {
        m_cachedTargetWindow = hwnd;
    }
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
    m_pipbongHeldVirtualKeys.insert(virtualKey);
    m_pipbongEverInjectedVirtualKeys.insert(virtualKey);
}

void ExecutionContext::noteSyntheticKeyUp(int virtualKey) {
    m_pipbongHeldVirtualKeys.erase(virtualKey);
}

bool ExecutionContext::hasPipbongSyntheticKeyDown(int virtualKey) const {
    return m_pipbongHeldVirtualKeys.find(virtualKey) != m_pipbongHeldVirtualKeys.end();
}

bool ExecutionContext::pipbongEverInjectedVirtualKey(int virtualKey) const {
    return m_pipbongEverInjectedVirtualKeys.find(virtualKey) != m_pipbongEverInjectedVirtualKeys.end();
}

void ExecutionContext::noteSyntheticMouseDown(MouseButton button) {
    const int raw = static_cast<int>(button);
    m_pipbongHeldMouseButtons.insert(raw);
    m_pipbongEverInjectedMouseButtons.insert(raw);
}

void ExecutionContext::noteSyntheticMouseUp(MouseButton button) {
    m_pipbongHeldMouseButtons.erase(static_cast<int>(button));
}

bool ExecutionContext::pipbongEverInjectedMouseButton(MouseButton button) const {
    return m_pipbongEverInjectedMouseButtons.find(static_cast<int>(button))
           != m_pipbongEverInjectedMouseButtons.end();
}

void ExecutionContext::restoreRunHeldInput() {
    if (!m_runKeyboardSessionActive) {
        return;
    }
    InputSimulator::restoreTrackedKeyboard(m_pipbongHeldVirtualKeys, m_runKeyboardSessionStart);
    InputSimulator::restoreTrackedMouseButtons(m_pipbongHeldMouseButtons);
}

void ExecutionContext::restoreRunKeyboard() {
    restoreRunHeldInput();
}

void ExecutionContext::endRunInputSession() {
    restoreRunHeldInput();
    m_runKeyboardSessionActive = false;
    m_pipbongHeldVirtualKeys.clear();
    m_pipbongHeldMouseButtons.clear();
    m_pipbongEverInjectedVirtualKeys.clear();
    m_pipbongEverInjectedMouseButtons.clear();
}

void ExecutionContext::endRunKeyboardSession() {
    endRunInputSession();
}
#endif
