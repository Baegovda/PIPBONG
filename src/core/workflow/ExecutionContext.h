#pragma once

#include "core/capture/ScreenCapture.h"

#include "core/input/InputSimulator.h"

#include <atomic>
#include <functional>
#include <opencv2/core.hpp>
#include <string>
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

    void setLogCallback(LogCallback callback);
    void log(const std::string& message) const;

    void setProgressCallback(ProgressCallback callback);
    void clearProgressCallback();
    void reportProgress(BlockProgressKind kind) const;

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

    void markDetectionFailure();
    void clearDetectionFailedFlag();
    bool detectionFailedThisRun() const;

    bool enterIfScope();
    void leaveIfScope();
    int ifNestingDepth() const;

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
    void restoreRunKeyboard();
    void endRunKeyboardSession();
#endif

private:
    struct ConsumedMatchRegion {
        cv::Point topLeft;
        cv::Size size;
    };

    LogCallback m_logCallback;
    ProgressCallback m_progressCallback;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    int m_imageFindMaxMissAttempts = 0;
    bool m_detectionFailedThisRun = false;
    int m_ifNestingDepth = 0;
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
    std::wstring m_targetWindowTitle;
    std::string m_projectDirectory;
#ifdef _WIN32
    bool m_runKeyboardSessionActive = false;
    SessionModifierSnapshot m_runKeyboardSessionStart;
    std::unordered_set<int> m_sbmHeldVirtualKeys;
#endif
};
