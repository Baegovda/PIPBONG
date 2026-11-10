#include "app/TargetWindowCenterPin.h"

#include <limits>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>

#include <cstdlib>
#endif

std::optional<QPoint> TargetWindowCenterPin::placement(const QRect& frame,const QRect& raw,
                                                       const QRect& monitor,bool forceSnap) {
    if(!frame.isValid() || !raw.isValid() || !monitor.isValid())return std::nullopt;
    const qint64 x=qint64(monitor.x())+(qint64(monitor.width())-frame.width())/2;
    const qint64 y=qint64(monitor.y())+(qint64(monitor.height())-frame.height())/2;
    if(!forceSnap && qAbs(qint64(frame.x())-x)<=2 && qAbs(qint64(frame.y())-y)<=2)return std::nullopt;
    const qint64 rawX=x-(qint64(frame.x())-raw.x()),rawY=y-(qint64(frame.y())-raw.y());
    if(rawX<std::numeric_limits<int>::min() || rawX>std::numeric_limits<int>::max()
        || rawY<std::numeric_limits<int>::min() || rawY>std::numeric_limits<int>::max())return std::nullopt;
    return QPoint(int(rawX),int(rawY));
}

bool TargetWindowCenterPin::syncWindow(HWND hwnd, bool forceSnap) {
#ifdef _WIN32
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

    RECT raw{};
    if(!GetWindowRect(hwnd,&raw))return false;
    const auto point=placement(QRect(dwmRect.left,dwmRect.top,winWidth,winHeight),
        QRect(raw.left,raw.top,raw.right-raw.left,raw.bottom-raw.top),
        QRect(monitorInfo.rcMonitor.left,monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right-monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom-monitorInfo.rcMonitor.top),forceSnap);
    if(!point)return false;

    return SetWindowPos(hwnd,
                        nullptr,
                        point->x(),
                        point->y(),
                        0,
                        0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
#else
    return false;
#endif
}
