#pragma once

#include <opencv2/core.hpp>
#include <optional>
#include <string>

#include <QPoint>
#include <QRect>

#ifdef _WIN32
#include <windows.h>
#endif

struct CaptureRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

/// Screen sub-region in percent of the virtual desktop (0–100).
struct PercentRegion {
    double x = 0.0;
    double y = 0.0;
    double width = 100.0;
    double height = 100.0;
};

enum class SearchArea {
    FullScreen,
    TargetWindow,
    CustomRegion,
    ScreenPercent
};

class ScreenCapture {
public:
    static void setTargetWindowTitle(const std::wstring& title);
    static std::wstring targetWindowTitle();
    /// Optional secondary binding (profile sub-target); used when the main title cannot be resolved.
    static void setSubTargetWindowTitle(const std::wstring& title);
    static std::wstring subTargetWindowTitle();

#ifdef _WIN32
    static HWND findTargetWindow();
    static void setTargetWindow(HWND hwnd);
    static HWND targetWindow();
    /// Reuse a foreground HWND from profile auto-switch for a short TTL (avoids EnumWindows).
    static void setForegroundHintWindow(HWND hwnd);
    /// True when a picked HWND or a resolvable title-based target window exists.
    static bool hasResolvableTargetWindow();
    /// Program setting: allow workflow runs when no target window is designated.
    static bool allowsRunWithoutTargetWindow();
    /// Brings the resolved target window to the foreground for reliable capture during workflow runs.
    static void activateTargetWindow();
    /// True when a visible top-level window title contains `binding` (case-sensitive substring).
    static bool hasVisibleWindowMatchingTitle(const std::wstring& binding);
    /// First visible top-level window whose title contains `binding` (substring match).
    static HWND findVisibleWindowMatchingTitle(const std::wstring& binding);
    static void warmupCapture();
#endif

    static cv::Mat captureFullScreen();
    static cv::Mat captureTargetWindow();
    static cv::Mat captureRegion(const CaptureRegion& region);
    /// Resolves percent ROI (0–100 of target window DWM bounds) to physical screen pixels.
    static CaptureRegion captureRegionFromPercent(const PercentRegion& percent);
    /// Converts window-relative percent ROI (0–100 of target DWM bounds) to physical screen pixels.
    static CaptureRegion resolveWindowPercentRegion(const PercentRegion& percent);
    static PercentRegion storeWindowPercentFromPhysical(const CaptureRegion& physical);
    static cv::Mat captureSearchArea(SearchArea area,
                                     const CaptureRegion& custom = {},
                                     const PercentRegion& percent = {});
    /// Haystack capture for ImageFind matching — prefers screen BitBlt for TargetWindow so
    /// templates from capturePhysicalRect match the same pixel grid as match test.
    static cv::Mat captureSearchAreaForImageFind(SearchArea area,
                                                 const CaptureRegion& custom = {},
                                                 const PercentRegion& percent = {});

    /// Maps haystack-relative match top-left to physical screen pixels for the given search ROI.
    /// For TargetWindow, pass haystack width/height so client-sized captures (PrintWindow /
    /// ClientOnly) map via ClientToScreen instead of DWM frame origin (avoids clicks outside the game).
    static cv::Point haystackTopLeftToPhysical(SearchArea area,
                                             const CaptureRegion& custom,
                                             const PercentRegion& percent,
                                             const cv::Point& haystackTopLeft,
                                             int haystackWidth = 0,
                                             int haystackHeight = 0);

    /// Physical screen rectangle for the search ROI (Win32 pixel coordinates).
    static bool searchAreaPhysicalRect(SearchArea area,
                                       const CaptureRegion& custom,
                                       const PercentRegion& percent,
                                       QRect& outRect);

    static bool isMostlyBlack(const cv::Mat& image, double meanThreshold = 8.0);

#ifdef _WIN32
    struct ScreenRect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool valid = false;
    };

    struct WindowBounds {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool valid = false;
    };

    static ScreenRect getTargetWindowScreenRect();

    struct TargetWindowInfo {
        bool found = false;
        std::wstring title;
        std::wstring className;
        std::wstring processPath;
        unsigned long long hwndValue = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int clientWidth = 0;
        int clientHeight = 0;
        bool visible = false;
        bool minimized = false;
        int monitorNumber = 0;
        int monitorWidth = 0;
        int monitorHeight = 0;
        int monitorDpi = 0;
    };

    /// Fills info for the currently resolved target window (`findTargetWindow`).
    static bool queryTargetWindowInfo(TargetWindowInfo& out);
    /// Fills info for a specific top-level window handle.
    static bool queryWindowInfo(HWND hwnd, TargetWindowInfo& out);

    /// DWM / Win32 client bounds in physical screen pixels.
    static WindowBounds physicalRectForWindow(HWND hwnd);
    /// Converts physical screen pixels to Qt logical coordinates for widget placement.
    static QRect logicalRectFromPhysical(int x, int y, int width, int height, HWND dpiReference = nullptr);
    static QPoint logicalPointFromPhysical(const QPoint& physical, HWND dpiReference = nullptr);
    static QPoint physicalPointFromLogical(const QPoint& logical, HWND dpiReference = nullptr);
    /// Captures a screen region in physical pixels (Win32 / BitBlt coordinate space).
    static cv::Mat captureScreenRect(int screenX, int screenY, int width, int height);
    static cv::Mat capturePhysicalRect(int x, int y, int width, int height);
    /// Template pick capture: BitBlt only (no DXGI) after overlay teardown; avoids cursor/overlay in PNGs.
    static cv::Mat capturePhysicalRectForTemplate(int x, int y, int width, int height);
#endif

private:
#ifdef _WIN32
    static cv::Mat captureWindow(HWND hwnd);
    static cv::Mat hBitmapToMat(HBITMAP hBitmap, int width, int height);
    static WindowBounds getWindowBounds(HWND hwnd);
    static cv::Mat captureWithPrintWindow(HWND hwnd, UINT flags);
    static cv::Mat captureWithClientBitBlt(HWND hwnd);
    static cv::Mat captureWithScreenBitBlt(HWND hwnd);
    static cv::Mat captureWithFullScreenCrop(HWND hwnd);
#endif

    static std::wstring s_targetTitle;
    static std::wstring s_subTargetTitle;
#ifdef _WIN32
    static HWND s_targetWindow;
    static HWND s_foregroundHintHwnd;
    static unsigned long long s_foregroundHintMs;
#endif
};
