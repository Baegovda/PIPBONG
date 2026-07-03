#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/// Pulsing border on the top-level window under the cursor during "창 지정" pick mode.
class WindowPickerHoverOverlay {
public:
#ifdef _WIN32
    static void updateHover(HWND hwnd);
#endif
    static void dismissAll();
};
