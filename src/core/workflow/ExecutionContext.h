#pragma once

#include "core/capture/ScreenCapture.h"

#include "core/input/InputSimulator.h"

#include <atomic>
#include <functional>
#include <optional>
#include <opencv2/core.hpp>
#include <string>
#include <unordered_map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

enum class BlockProgressKind {
    ImageFindMiss,
    ImageFindSuccess
};

class ExecutionContext {
public:
    using LogCallback = std::function<void(const std::string& message)>;
    using ProgressCallback = std::function<void(BlockProgressKind kind)>;
    using PointerFeedbackCallback = std::function<void(int clientX, int clientY)>;

    void setLogCallback(LogCallback callback);
    void log(const std::string& message) const;

    void setProgressCallback(ProgressCallback callback);
    void clearProgressCallback();
    void reportProgress(BlockProgressKind kind) const;

    void setPointerFeedbackCallback(PointerFeedbackCallback callback);
    void clearPointerFeedbackCallback();
    void setPointerVisualFeedback(bool enabled);
    bool pointerVisualFeedback() const;
    void reportPointerFeedback(int clientX, int clientY) const;

    void requestStop();
    bool shouldStop() const;
    void resetStop();

    void setPaused(bool paused);
    bool isPaused() const;
    void togglePaused();

    /// Sleeps in small steps; returns false when stop is requested.
    bool interruptibleSleepMs(int delayMs);
    /// Waits while paused until resumed or stopped; returns false when stopped.
    bool waitWhilePaused();

    void setImageFindMaxMissAttempts(int attempts);
    int imageFindMaxMissAttempts() const;

    void setImageFindPollAttempt(int attempt);
    int imageFindPollAttempt() const;

    void markDetectionFailure();
    void clearDetectionFailedFlag();
    bool detectionFailedThisRun() const;

    bool imageFindDeferRetryUsed(int blockIndex) const;
    void markImageFindDeferRetryUsed(int blockIndex);
    void clearImageFindDeferRetryUsed(int blockIndex);

    void incrementImageFindReturnToPreviousCount(int blockIndex);
    void incrementImageFindRetryAfterNextCount(int blockIndex);
    int imageFindReturnToPreviousCount(int blockIndex) const;
    int imageFindRetryAfterNextCount(int blockIndex) const;

    static constexpr int kMaxWorkflowNestingDepth = 8;

    bool enterNestingScope();
    void leaveNestingScope();

    bool hasLastMatch() const;
    cv::Point lastMatchPoint() const;
    bool hasLastMatchScreenPoint() const;
    cv::Point lastMatchScreenPoint() const;
    double lastMatchConfidence() const;
    double lastMatchThreshold() const;
    bool hasLastMatchPreview() const;
    cv::Mat lastMatchPreview() const;
    void setLastMatch(const cv::Point& point,
                      double confidence,
                      const cv::Mat& previewBgr = cv::Mat(),
                      double matchThreshold = 0.0);
    void setLastMatchScreenPoint(const cv::Point& point);
    void clearLastMatch();

    void setLastMatchAttempt(double matchThreshold, double peakConfidence, const cv::Point& clientPoint, bool hasClientPoint);
    bool hasLastMatchAttempt() const;
    double lastMatchAttemptConfidence() const;
    double lastMatchAttemptThreshold() const;
    bool hasLastMatchAttemptPoint() const;
    cv::Point lastMatchAttemptClientPoint() const;
    void clearLastMatchAttempt();

    bool isMatchRegionConsumed(const cv::Point& topLeft, const cv::Size& size) const;
    bool hasConsumedMatchRegions() const;
    /// Records a matched region in physical screen pixels (see ScreenCapture::haystackTopLeftToPhysical).
    void consumeMatchRegion(const cv::Point& topLeft, const cv::Size& size);
    void clearConsumedMatchRegions();

    void setTriggerMonitorBlockIndex(int blockIndex);
    int triggerMonitorBlockIndex() const;

    void setImageFindPrimedBlockIndex(int blockIndex);
    bool consumeImageFindPrimedBlockIndex(int blockIndex);

    void setRoiCorrectionSession(bool eligible, bool featureGlobal);
    bool roiCorrectionSessionEligible() const;
    bool featureRoiCorrectionGlobal() const;
    bool shouldUseRoiCorrectionForBlock(bool blockRoiCorrection) const;

    void setRunLoopNumber(int loopNumber);
    int runLoopNumber() const;

    void setActiveBlockIndex(int blockIndex);
    int activeBlockIndex() const;

    void setCorrectedRoi(int blockIndex, const CaptureRegion& region);
    std::optional<CaptureRegion> correctedRoi(int blockIndex) const;
    void clearCorrectedRois();

    void setTargetWindowTitle(const std::wstring& title);
    std::wstring targetWindowTitle() const;

#ifdef _WIN32
    HWND targetWindow() const;
#endif

    std::string resolvePath(const std::string& relativePath) const;
    void setProjectDirectory(const std::string& path);
    std::string projectDirectory() const;

#ifdef _WIN32
    void beginRunKeyboardSessionIfNeeded();
    void noteSyntheticKeyDown(int virtualKey);
    void noteSyntheticKeyUp(int virtualKey);
    void noteSyntheticMouseDown(MouseButton button);
    void noteSyntheticMouseUp(MouseButton button);
    void restoreRunHeldInput();
    void restoreRunKeyboard();
    void endRunInputSession();
    void endRunKeyboardSession();
#endif

private:
    struct ConsumedMatchRegion {
        cv::Point topLeft;
        cv::Size size;
    };

    LogCallback m_logCallback;
    ProgressCallback m_progressCallback;
    PointerFeedbackCallback m_pointerFeedbackCallback;
    bool m_pointerVisualFeedback = true;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    int m_imageFindMaxMissAttempts = 0;
    int m_imageFindPollAttempt = 0;
    bool m_detectionFailedThisRun = false;
    std::unordered_set<int> m_imageFindDeferRetryUsedBlockIndexes;
    std::unordered_map<int, int> m_imageFindReturnToPreviousCounts;
    std::unordered_map<int, int> m_imageFindRetryAfterNextCounts;
    int m_nestingDepth = 0;
    bool m_hasLastMatch = false;
    cv::Point m_lastMatchPoint;
    bool m_hasLastMatchScreenPoint = false;
    cv::Point m_lastMatchScreenPoint;
    double m_lastMatchConfidence = 0.0;
    double m_lastMatchThreshold = 0.0;
    cv::Mat m_lastMatchPreview;
    bool m_hasLastMatchAttempt = false;
    double m_lastMatchAttemptConfidence = 0.0;
    double m_lastMatchAttemptThreshold = 0.0;
    bool m_hasLastMatchAttemptPoint = false;
    cv::Point m_lastMatchAttemptClientPoint;
    std::vector<ConsumedMatchRegion> m_consumedMatchRegions;
    int m_triggerMonitorBlockIndex = -1;
    int m_imageFindPrimedBlockIndex = -1;
    bool m_roiCorrectionSessionEligible = false;
    bool m_featureRoiCorrectionGlobal = false;
    int m_runLoopNumber = 1;
    int m_activeBlockIndex = -1;
    std::unordered_map<int, CaptureRegion> m_correctedRoisByBlockIndex;
    std::wstring m_targetWindowTitle;
    std::string m_projectDirectory;
#ifdef _WIN32
    bool m_runKeyboardSessionActive = false;
    SessionModifierSnapshot m_runKeyboardSessionStart;
    std::unordered_set<int> m_pipbongHeldVirtualKeys;
    std::unordered_set<int> m_pipbongHeldMouseButtons;
#endif
};
