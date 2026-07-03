#include "core/capture/ScreenCapture.h"

#include "core/capture/DxgiScreenCapture.h"

#include <opencv2/imgproc.hpp>

#ifdef _WIN32
#include <dwmapi.h>
#include <windows.h>
#endif

#include <algorithm>

#ifdef _WIN32
namespace {

using PhysicalToLogicalForDpiFn = BOOL(WINAPI*)(HWND, LPPOINT);
using LogicalToPhysicalForDpiFn = BOOL(WINAPI*)(HWND, LPPOINT);

PhysicalToLogicalForDpiFn physicalToLogicalForDpiFn() {
    static const auto fn = reinterpret_cast<PhysicalToLogicalForDpiFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "PhysicalToLogicalPointForPerMonitorDPI"));
    return fn;
}

LogicalToPhysicalForDpiFn logicalToPhysicalForDpiFn() {
    static const auto fn = reinterpret_cast<LogicalToPhysicalForDpiFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "LogicalToPhysicalPointForPerMonitorDPI"));
    return fn;
}

HWND dpiReferenceWindow(HWND dpiReference) {
    if (dpiReference && IsWindow(dpiReference)) {
        return dpiReference;
    }
    return GetDesktopWindow();
}

bool convertPhysicalToLogical(HWND dpiReference, POINT& point) {
    if (const auto fn = physicalToLogicalForDpiFn()) {
        return fn(dpiReferenceWindow(dpiReference), &point) != FALSE;
    }
    return false;
}

bool convertLogicalToPhysical(HWND dpiReference, POINT& point) {
    if (const auto fn = logicalToPhysicalForDpiFn()) {
        return fn(dpiReferenceWindow(dpiReference), &point) != FALSE;
    }
    return false;
}

void restoreForegroundWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    if (GetForegroundWindow() == hwnd) {
        return;
    }

    // Never use AttachThreadInput here — detach clears modifier keys the user is still
    // holding physically (Shift-run in games) even when focus was already on the target.
    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
}

using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

GetDpiForWindowFn getDpiForWindowFn() {
    static const auto fn =
        reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    return fn;
}

struct MonitorIndexContext {
    HMONITOR target = nullptr;
    int index = 0;
    int matchedNumber = 0;
};

BOOL CALLBACK monitorIndexEnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM param) {
    auto* ctx = reinterpret_cast<MonitorIndexContext*>(param);
    ctx->index++;
    if (monitor == ctx->target) {
        ctx->matchedNumber = ctx->index;
    }
    return TRUE;
}

void fillMonitorInfoForWindow(HWND hwnd, ScreenCapture::TargetWindowInfo& out) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return;
    }

    MONITORINFOEX monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&monitorInfo))) {
        return;
    }

    out.monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    out.monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

    MonitorIndexContext indexContext{monitor, 0, 0};
    EnumDisplayMonitors(nullptr, nullptr, monitorIndexEnumProc, reinterpret_cast<LPARAM>(&indexContext));
    out.monitorNumber = indexContext.matchedNumber;

    if (const auto getDpiForWindow = getDpiForWindowFn()) {
        out.monitorDpi = static_cast<int>(getDpiForWindow(hwnd));
    } else {
        HDC monitorDc = CreateDCW(L"DISPLAY", monitorInfo.szDevice, nullptr, nullptr);
        if (monitorDc) {
            out.monitorDpi = GetDeviceCaps(monitorDc, LOGPIXELSX);
            DeleteDC(monitorDc);
        }
    }
}

cv::Mat bitBltPhysicalRectGdi(int screenX, int screenY, int width, int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    HDC screenDc = GetDC(nullptr);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        ReleaseDC(nullptr, screenDc);
        return {};
    }

    HDC memDc = CreateCompatibleDC(screenDc);
    HGDIOBJ old = SelectObject(memDc, dib);
    BitBlt(memDc, 0, 0, width, height, screenDc, screenX, screenY, SRCCOPY | CAPTUREBLT);
    SelectObject(memDc, old);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    const cv::Mat bgra(height, width, CV_8UC4, bits);
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    DeleteObject(dib);
    return bgr;
}

} // namespace
#endif

std::wstring ScreenCapture::s_targetTitle;
#ifdef _WIN32
HWND ScreenCapture::s_targetWindow = nullptr;

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#endif

void ScreenCapture::setTargetWindowTitle(const std::wstring& title) {
#ifdef _WIN32
    if (s_targetTitle != title) {
        s_targetTitle = title;
        s_targetWindow = nullptr;
        return;
    }
#endif
    s_targetTitle = title;
}

std::wstring ScreenCapture::targetWindowTitle() {
    return s_targetTitle;
}

#ifdef _WIN32
HWND ScreenCapture::findTargetWindow() {
    if (s_targetWindow && IsWindow(s_targetWindow)) {
        return s_targetWindow;
    }

    struct EnumData {
        std::wstring title;
        HWND result = nullptr;
    } data{s_targetTitle, nullptr};

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* enumData = reinterpret_cast<EnumData*>(lParam);
            if (!IsWindowVisible(hwnd)) {
                return TRUE;
            }
            wchar_t buffer[512]{};
            GetWindowTextW(hwnd, buffer, 512);
            std::wstring title(buffer);
            if (title.find(enumData->title) != std::wstring::npos) {
                enumData->result = hwnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data));

    s_targetWindow = data.result;
    return s_targetWindow;
}

void ScreenCapture::setTargetWindow(HWND hwnd) {
    s_targetWindow = hwnd;
}

HWND ScreenCapture::targetWindow() {
    return s_targetWindow;
}

void ScreenCapture::activateTargetWindow() {
    const HWND hwnd = findTargetWindow();
    if (!hwnd) {
        return;
    }
    if (GetForegroundWindow() == hwnd) {
        return;
    }
    restoreForegroundWindow(hwnd);
}

void ScreenCapture::warmupCapture() {
    if (HWND hwnd = findTargetWindow()) {
        captureWithScreenBitBlt(hwnd);
        return;
    }

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    capturePhysicalRect(x, y, 32, 32);
}
#endif

bool ScreenCapture::isMostlyBlack(const cv::Mat& image, double meanThreshold) {
    if (image.empty()) {
        return true;
    }

    const int channels = image.channels();
    if (channels != 1 && channels != 3 && channels != 4) {
        return true;
    }

    const int step = std::max(1, std::min(image.cols, image.rows) / 64);
    double sum = 0.0;
    int count = 0;

    for (int y = 0; y < image.rows; y += step) {
        const uchar* row = image.ptr<uchar>(y);
        if (channels == 1) {
            for (int x = 0; x < image.cols; x += step) {
                sum += row[x];
                ++count;
            }
            continue;
        }

        const int pixelStep = step * channels;
        for (int x = 0; x < image.cols * channels; x += pixelStep) {
            const int b = row[x];
            const int g = row[x + 1];
            const int r = row[x + 2];
            sum += static_cast<double>((b + g + r) / 3);
            ++count;
        }
    }

    return count == 0 || (sum / count) < meanThreshold;
}

cv::Mat ScreenCapture::captureFullScreen() {
#ifdef _WIN32
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    HGDIOBJ old = SelectObject(memDc, bitmap);
    BitBlt(memDc, 0, 0, width, height, screenDc, x, y, SRCCOPY | CAPTUREBLT);
    cv::Mat result = hBitmapToMat(bitmap, width, height);
    SelectObject(memDc, old);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return result;
#else
    return {};
#endif
}

cv::Mat ScreenCapture::captureTargetWindow() {
#ifdef _WIN32
    HWND hwnd = findTargetWindow();
    if (!hwnd) {
        return {};
    }
    return captureWindow(hwnd);
#else
    return {};
#endif
}

cv::Mat ScreenCapture::captureRegion(const CaptureRegion& region) {
    cv::Mat full = captureFullScreen();
    if (full.empty() || region.width <= 0 || region.height <= 0) {
        return {};
    }
#ifdef _WIN32
    const int originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int originY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int x = std::max(0, region.x - originX);
    const int y = std::max(0, region.y - originY);
#else
    const int x = std::max(0, region.x);
    const int y = std::max(0, region.y);
#endif
    const int w = std::min(region.width, full.cols - x);
    const int h = std::min(region.height, full.rows - y);
    if (w <= 0 || h <= 0) {
        return {};
    }
    return full(cv::Rect(x, y, w, h)).clone();
}

CaptureRegion ScreenCapture::captureRegionFromPercent(const PercentRegion& percent) {
    return resolveWindowPercentRegion(percent);
}

CaptureRegion ScreenCapture::resolveWindowPercentRegion(const PercentRegion& percent) {
    CaptureRegion region;
#ifdef _WIN32
    const ScreenRect target = getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        return region;
    }

    const double clampedX = std::clamp(percent.x, 0.0, 100.0);
    const double clampedY = std::clamp(percent.y, 0.0, 100.0);
    const double maxWidth = std::max(0.0, 100.0 - clampedX);
    const double maxHeight = std::max(0.0, 100.0 - clampedY);
    const double clampedW = std::clamp(percent.width, 0.01, maxWidth);
    const double clampedH = std::clamp(percent.height, 0.01, maxHeight);

    region.x = target.x + static_cast<int>(std::lround(target.width * clampedX / 100.0));
    region.y = target.y + static_cast<int>(std::lround(target.height * clampedY / 100.0));
    region.width = std::max(1, static_cast<int>(std::lround(target.width * clampedW / 100.0)));
    region.height = std::max(1, static_cast<int>(std::lround(target.height * clampedH / 100.0)));
#endif
    return region;
}

PercentRegion ScreenCapture::storeWindowPercentFromPhysical(const CaptureRegion& physical) {
    PercentRegion percent;
#ifdef _WIN32
    const ScreenRect target = getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0 || physical.width <= 0
        || physical.height <= 0) {
        return percent;
    }
    percent.x = (physical.x - target.x) * 100.0 / target.width;
    percent.y = (physical.y - target.y) * 100.0 / target.height;
    percent.width = physical.width * 100.0 / target.width;
    percent.height = physical.height * 100.0 / target.height;
#endif
    return percent;
}

cv::Mat ScreenCapture::captureSearchArea(SearchArea area,
                                         const CaptureRegion& custom,
                                         const PercentRegion& percent) {
    switch (area) {
    case SearchArea::FullScreen:
        return captureFullScreen();
    case SearchArea::TargetWindow:
        return captureTargetWindow();
    case SearchArea::CustomRegion:
        return captureRegion(custom);
    case SearchArea::ScreenPercent:
        return captureRegion(captureRegionFromPercent(percent));
    }
    return {};
}

cv::Mat ScreenCapture::captureSearchAreaForImageFind(SearchArea area,
                                                     const CaptureRegion& custom,
                                                     const PercentRegion& percent) {
#ifdef _WIN32
    if (area == SearchArea::TargetWindow) {
        HWND hwnd = targetWindow();
        if (!hwnd || !IsWindow(hwnd)) {
            hwnd = findTargetWindow();
        }
        if (!hwnd) {
            return {};
        }

        const auto acceptableCapture = [](const cv::Mat& mat) {
            return !mat.empty() && !isMostlyBlack(mat);
        };

        // Screen BitBlt matches templates captured via capturePhysicalRect (match test path).
        // PrintWindow client buffers often differ and break template matching during workflow runs.
        cv::Mat mat = captureWithScreenBitBlt(hwnd);
        if (acceptableCapture(mat)) {
            return mat;
        }

        mat = captureWithFullScreenCrop(hwnd);
        if (acceptableCapture(mat)) {
            return mat;
        }

        return captureWindow(hwnd);
    }

    if (area == SearchArea::CustomRegion) {
        if (custom.width <= 0 || custom.height <= 0) {
            return {};
        }
        cv::Mat mat = capturePhysicalRect(custom.x, custom.y, custom.width, custom.height);
        if (!mat.empty() && !isMostlyBlack(mat)) {
            return mat;
        }
        return captureRegion(custom);
    }
#endif

    return captureSearchArea(area, custom, percent);
}

cv::Point ScreenCapture::haystackTopLeftToPhysical(SearchArea area,
                                                   const CaptureRegion& custom,
                                                   const PercentRegion& percent,
                                                   const cv::Point& haystackTopLeft) {
#ifdef _WIN32
    switch (area) {
    case SearchArea::TargetWindow: {
        const ScreenRect target = getTargetWindowScreenRect();
        if (target.valid) {
            return cv::Point(target.x + haystackTopLeft.x, target.y + haystackTopLeft.y);
        }
        break;
    }
    case SearchArea::CustomRegion:
        return cv::Point(custom.x + haystackTopLeft.x, custom.y + haystackTopLeft.y);
    case SearchArea::ScreenPercent: {
        const CaptureRegion resolved = resolveWindowPercentRegion(percent);
        return cv::Point(resolved.x + haystackTopLeft.x, resolved.y + haystackTopLeft.y);
    }
    case SearchArea::FullScreen: {
        const int originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int originY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        return cv::Point(originX + haystackTopLeft.x, originY + haystackTopLeft.y);
    }
    }
#else
    (void)area;
    (void)custom;
    (void)percent;
#endif
    return haystackTopLeft;
}

bool ScreenCapture::searchAreaPhysicalRect(SearchArea area,
                                           const CaptureRegion& custom,
                                           const PercentRegion& percent,
                                           QRect& outRect) {
#ifdef _WIN32
    switch (area) {
    case SearchArea::TargetWindow: {
        const ScreenRect target = getTargetWindowScreenRect();
        if (!target.valid || target.width <= 0 || target.height <= 0) {
            return false;
        }
        outRect = QRect(target.x, target.y, target.width, target.height);
        return true;
    }
    case SearchArea::CustomRegion:
        if (custom.width <= 0 || custom.height <= 0) {
            return false;
        }
        outRect = QRect(custom.x, custom.y, custom.width, custom.height);
        return true;
    case SearchArea::ScreenPercent: {
        const CaptureRegion resolved = resolveWindowPercentRegion(percent);
        if (resolved.width <= 0 || resolved.height <= 0) {
            return false;
        }
        outRect = QRect(resolved.x, resolved.y, resolved.width, resolved.height);
        return true;
    }
    case SearchArea::FullScreen:
        outRect = QRect(GetSystemMetrics(SM_XVIRTUALSCREEN),
                        GetSystemMetrics(SM_YVIRTUALSCREEN),
                        GetSystemMetrics(SM_CXVIRTUALSCREEN),
                        GetSystemMetrics(SM_CYVIRTUALSCREEN));
        return outRect.width() > 0 && outRect.height() > 0;
    }
#else
    (void)area;
    (void)custom;
    (void)percent;
    (void)outRect;
#endif
    return false;
}

#ifdef _WIN32
ScreenCapture::WindowBounds ScreenCapture::physicalRectForWindow(HWND hwnd) {
    return getWindowBounds(hwnd);
}

QPoint ScreenCapture::logicalPointFromPhysical(const QPoint& physical, HWND dpiReference) {
    POINT point{physical.x(), physical.y()};
    if (convertPhysicalToLogical(dpiReference, point)) {
        return QPoint(point.x, point.y);
    }
    return physical;
}

QPoint ScreenCapture::physicalPointFromLogical(const QPoint& logical, HWND dpiReference) {
    POINT point{logical.x(), logical.y()};
    if (convertLogicalToPhysical(dpiReference, point)) {
        return QPoint(point.x, point.y);
    }
    return logical;
}

QRect ScreenCapture::logicalRectFromPhysical(int x, int y, int width, int height, HWND dpiReference) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    const QPoint topLeft = logicalPointFromPhysical(QPoint(x, y), dpiReference);
    const QPoint bottomRight = logicalPointFromPhysical(QPoint(x + width, y + height), dpiReference);
    return QRect(topLeft, bottomRight).normalized();
}

cv::Mat ScreenCapture::capturePhysicalRect(int x, int y, int width, int height) {
    return captureScreenRect(x, y, width, height);
}

cv::Mat ScreenCapture::capturePhysicalRectForTemplate(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    DxgiScreenCapture::resetSession();

    // Do not call ShowCursor(FALSE) here — it corrupts the global cursor display count and can
    // leave the pointer invisible over the main window. Screen BitBlt does not include the cursor.
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    HGDIOBJ old = SelectObject(memDc, bitmap);
    BitBlt(memDc, 0, 0, width, height, screenDc, x, y, SRCCOPY | CAPTUREBLT);
    cv::Mat result = hBitmapToMat(bitmap, width, height);
    SelectObject(memDc, old);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    return result;
}

ScreenCapture::ScreenRect ScreenCapture::getTargetWindowScreenRect() {
    ScreenRect rect;
    HWND hwnd = findTargetWindow();
    if (!hwnd) {
        return rect;
    }
    const WindowBounds bounds = getWindowBounds(hwnd);
    rect.x = bounds.x;
    rect.y = bounds.y;
    rect.width = bounds.width;
    rect.height = bounds.height;
    rect.valid = bounds.valid;
    return rect;
}

bool ScreenCapture::queryWindowInfo(HWND hwnd, TargetWindowInfo& out) {
    out = {};
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    out.found = true;
    out.hwndValue = static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(hwnd));

    wchar_t titleBuffer[512]{};
    GetWindowTextW(hwnd, titleBuffer, 512);
    out.title = titleBuffer;

    wchar_t classBuffer[256]{};
    GetClassNameW(hwnd, classBuffer, 256);
    out.className = classBuffer;

    const WindowBounds bounds = getWindowBounds(hwnd);
    if (bounds.valid) {
        out.x = bounds.x;
        out.y = bounds.y;
        out.width = bounds.width;
        out.height = bounds.height;
    }

    RECT clientRect{};
    if (GetClientRect(hwnd, &clientRect)) {
        out.clientWidth = clientRect.right - clientRect.left;
        out.clientHeight = clientRect.bottom - clientRect.top;
    }

    out.visible = IsWindowVisible(hwnd) != FALSE;
    out.minimized = IsIconic(hwnd) != FALSE;

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != 0) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (process) {
            wchar_t pathBuffer[MAX_PATH]{};
            DWORD pathLength = MAX_PATH;
            if (QueryFullProcessImageNameW(process, 0, pathBuffer, &pathLength)) {
                out.processPath = pathBuffer;
            }
            CloseHandle(process);
        }
    }

    fillMonitorInfoForWindow(hwnd, out);

    return true;
}

bool ScreenCapture::queryTargetWindowInfo(TargetWindowInfo& out) {
    return queryWindowInfo(findTargetWindow(), out);
}

cv::Mat ScreenCapture::captureScreenRect(int screenX, int screenY, int width, int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    cv::Mat dxgi = DxgiScreenCapture::capturePhysicalRect(screenX, screenY, width, height);
    if (!dxgi.empty() && !isMostlyBlack(dxgi)) {
        return dxgi;
    }

    cv::Mat result = bitBltPhysicalRectGdi(screenX, screenY, width, height);

    if (isMostlyBlack(result)) {
        CaptureRegion region;
        region.x = screenX - GetSystemMetrics(SM_XVIRTUALSCREEN);
        region.y = screenY - GetSystemMetrics(SM_YVIRTUALSCREEN);
        region.width = width;
        region.height = height;
        cv::Mat fallback = captureRegion(region);
        if (!fallback.empty() && !isMostlyBlack(fallback)) {
            return fallback;
        }
    }

    return result;
}
#endif

#ifdef _WIN32
ScreenCapture::WindowBounds ScreenCapture::getWindowBounds(HWND hwnd) {
    WindowBounds bounds;

    RECT dwmRect{};
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwmRect, sizeof(dwmRect)))) {
        bounds.x = dwmRect.left;
        bounds.y = dwmRect.top;
        bounds.width = dwmRect.right - dwmRect.left;
        bounds.height = dwmRect.bottom - dwmRect.top;
        bounds.valid = bounds.width > 0 && bounds.height > 0;
        if (bounds.valid) {
            return bounds;
        }
    }

    RECT windowRect{};
    if (GetWindowRect(hwnd, &windowRect)) {
        bounds.x = windowRect.left;
        bounds.y = windowRect.top;
        bounds.width = windowRect.right - windowRect.left;
        bounds.height = windowRect.bottom - windowRect.top;
        bounds.valid = bounds.width > 0 && bounds.height > 0;
    }

    return bounds;
}

cv::Mat ScreenCapture::captureWithPrintWindow(HWND hwnd, UINT flags) {
    RECT rect{};
    if (!GetClientRect(hwnd, &rect)) {
        return {};
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return {};
    }

    HDC windowDc = GetDC(hwnd);
    HDC memDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ old = SelectObject(memDc, bitmap);

    const BOOL ok = PrintWindow(hwnd, memDc, flags);
    cv::Mat result;
    if (ok) {
        result = hBitmapToMat(bitmap, width, height);
    }

    SelectObject(memDc, old);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(hwnd, windowDc);
    return result;
}

cv::Mat ScreenCapture::captureWithClientBitBlt(HWND hwnd) {
    RECT rect{};
    if (!GetClientRect(hwnd, &rect)) {
        return {};
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return {};
    }

    HDC windowDc = GetDC(hwnd);
    HDC memDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ old = SelectObject(memDc, bitmap);
    BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
    cv::Mat result = hBitmapToMat(bitmap, width, height);
    SelectObject(memDc, old);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(hwnd, windowDc);
    return result;
}

cv::Mat ScreenCapture::captureWithScreenBitBlt(HWND hwnd) {
    if (IsIconic(hwnd)) {
        return {};
    }

    const WindowBounds bounds = getWindowBounds(hwnd);
    if (!bounds.valid) {
        return {};
    }

    cv::Mat dxgi = DxgiScreenCapture::capturePhysicalRect(bounds.x, bounds.y, bounds.width, bounds.height);
    if (!dxgi.empty() && !isMostlyBlack(dxgi)) {
        return dxgi;
    }

    return bitBltPhysicalRectGdi(bounds.x, bounds.y, bounds.width, bounds.height);
}

cv::Mat ScreenCapture::captureWithFullScreenCrop(HWND hwnd) {
    const WindowBounds bounds = getWindowBounds(hwnd);
    if (!bounds.valid) {
        return {};
    }

    cv::Mat full = captureFullScreen();
    if (full.empty()) {
        return {};
    }

    const int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int x = bounds.x - virtualX;
    const int y = bounds.y - virtualY;
    const int w = std::min(bounds.width, full.cols - x);
    const int h = std::min(bounds.height, full.rows - y);
    if (x < 0 || y < 0 || w <= 0 || h <= 0) {
        return {};
    }

    return full(cv::Rect(x, y, w, h)).clone();
}

cv::Mat ScreenCapture::captureWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return {};
    }

    const auto tryCapture = [](const cv::Mat& mat) { return !mat.empty() && !isMostlyBlack(mat); };

    cv::Mat mat = captureWithPrintWindow(hwnd, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (tryCapture(mat)) {
        return mat;
    }

    mat = captureWithScreenBitBlt(hwnd);
    if (tryCapture(mat)) {
        return mat;
    }

    mat = captureWithPrintWindow(hwnd, PW_CLIENTONLY);
    if (tryCapture(mat)) {
        return mat;
    }

    mat = captureWithClientBitBlt(hwnd);
    if (tryCapture(mat)) {
        return mat;
    }

    mat = captureWithFullScreenCrop(hwnd);
    if (tryCapture(mat)) {
        return mat;
    }

    return {};
}

cv::Mat ScreenCapture::hBitmapToMat(HBITMAP hBitmap, int width, int height) {
    BITMAP bmp{};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    cv::Mat mat(height, width, CV_8UC4);
    HDC dc = GetDC(nullptr);
    GetDIBits(dc, hBitmap, 0, height, mat.data, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);

    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}
#endif
