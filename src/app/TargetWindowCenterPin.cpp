#include "app/TargetWindowCenterPin.h"

#include "core/capture/ScreenCapture.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>

#include <cstdlib>
#endif

bool TargetWindowCenterPin::sync() {
#ifdef _WIN32
    HWND hwnd = ScreenCapture::targetWindow();
    if (!hwnd || !IsWindow(hwnd) || IsIconic(hwnd)) {
        if (hwnd) {
            ScreenCapture::setTargetWindow(nullptr);
        }
        hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            ScreenCapture::setTargetWindow(hwnd);
        }
    }
    if (!hwnd || !IsWindow(hwnd) || IsIconic(hwnd)) {
        return false;
    }

    RECT dwmRect{};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwmRect, sizeof(dwmRect)))) {
        return false;
    }

    const int winWidth = dwmRect.right - dwmRect.left;
    const int winHeight = dwmRect.bottom - dwmRect.top;
    if (winWidth <= 0 || winHeight <= 0) {
        return false;
    }

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return false;
    }

    const int centerX = (monitorInfo.rcMonitor.left + monitorInfo.rcMonitor.right) / 2;
    const int centerY = (monitorInfo.rcMonitor.top + monitorInfo.rcMonitor.bottom) / 2;
    const int targetX = centerX - winWidth / 2;
    const int targetY = centerY - winHeight / 2;

    constexpr int kTolerancePx = 2;
    if (std::abs(dwmRect.left - targetX) <= kTolerancePx
        && std::abs(dwmRect.top - targetY) <= kTolerancePx) {
        return false;
    }

    return SetWindowPos(hwnd,
                        nullptr,
                        targetX,
                        targetY,
                        0,
                        0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
#else
    return false;
#endif
}
