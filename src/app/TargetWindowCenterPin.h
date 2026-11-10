#pragma once
#include <QRect>
#include <QPoint>
#include <optional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/// Keeps the designated target window centered on its current monitor (Win32).
class TargetWindowCenterPin {
public:
    /// Pure physical-pixel calculation. Preserve the visible-frame/raw-window offset.
    static std::optional<QPoint> placement(const QRect& visibleFrame,const QRect& rawWindow,
                                           const QRect& monitor,bool forceSnap=false);

#ifdef _WIN32
    /// Center a specific top-level window on its current monitor.
    /// When \a forceSnap is true, always reposition (e.g. user just enabled center pin).
    static bool syncWindow(HWND hwnd, bool forceSnap = false);
#endif
};
