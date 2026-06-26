#pragma once

#include <functional>
#include <string>

class QWidget;

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class WindowPicker {
public:
    struct Result {
        bool accepted = false;
#ifdef _WIN32
        HWND hwnd = nullptr;
#endif
        std::wstring title;
    };

    using Completion = std::function<void(const Result&)>;

    /// Global click-to-pick: crosshair cursor, left-click selects top-level window, Esc cancels.
    static void startPick(QWidget* hostWidget, Completion callback);
    static void cancelPick();
};
