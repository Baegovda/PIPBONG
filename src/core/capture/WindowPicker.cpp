#include "core/capture/WindowPicker.h"

#include "ui/WindowPickerHoverOverlay.h"

#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
namespace {

struct PickState {
    HHOOK mouseHook = nullptr;
    HHOOK keyboardHook = nullptr;
    HCURSOR crossCursor = nullptr;
    WindowPicker::Completion callback;
    QPointer<QWidget> host;
};

std::unique_ptr<PickState> g_state;

bool isOwnProcessWindow(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    return windowPid == GetCurrentProcessId();
}

HWND topLevelWindowAtPoint(POINT pt) {
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) {
        return nullptr;
    }
    return GetAncestor(hwnd, GA_ROOT);
}

bool isPickableWindow(HWND hwnd) {
    return hwnd && IsWindow(hwnd) && IsWindowVisible(hwnd) && !IsIconic(hwnd) && !isOwnProcessWindow(hwnd);
}

void updateHoverAtPoint(POINT pt) {
    HWND hwnd = topLevelWindowAtPoint(pt);
    if (isPickableWindow(hwnd)) {
        WindowPickerHoverOverlay::updateHover(hwnd);
    } else {
        WindowPickerHoverOverlay::dismissAll();
    }
}

std::wstring windowTitleFromHandle(HWND hwnd) {
    if (!hwnd) {
        return {};
    }
    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, 512);
    if (title[0] != L'\0') {
        return title;
    }
    wchar_t className[256]{};
    GetClassNameW(hwnd, className, 256);
    return className;
}

void teardownHooks(PickState& state) {
    if (state.mouseHook) {
        UnhookWindowsHookEx(state.mouseHook);
        state.mouseHook = nullptr;
    }
    if (state.keyboardHook) {
        UnhookWindowsHookEx(state.keyboardHook);
        state.keyboardHook = nullptr;
    }
    if (state.crossCursor) {
        DestroyCursor(state.crossCursor);
        state.crossCursor = nullptr;
    }
}

void finishPick(WindowPicker::Result result) {
    if (!g_state) {
        return;
    }

    WindowPicker::Completion callback = std::move(g_state->callback);
    teardownHooks(*g_state);
    g_state.reset();
    WindowPickerHoverOverlay::dismissAll();

    QTimer::singleShot(0, qApp, [callback = std::move(callback), result = std::move(result)]() mutable {
        if (callback) {
            callback(result);
        }
    });
}

LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_state) {
        return CallNextHookEx(g_state ? g_state->mouseHook : nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (wParam == WM_MOUSEMOVE) {
        if (g_state->crossCursor) {
            SetCursor(g_state->crossCursor);
        }
        updateHoverAtPoint(info->pt);
        return CallNextHookEx(g_state->mouseHook, code, wParam, lParam);
    }

    if (wParam == WM_LBUTTONDOWN) {
        WindowPicker::Result result;
        HWND hwnd = topLevelWindowAtPoint(info->pt);
        if (isPickableWindow(hwnd)) {
            result.accepted = true;
            result.hwnd = hwnd;
            result.title = windowTitleFromHandle(hwnd);
        }
        finishPick(result);
        return 1;
    }

    if (wParam == WM_RBUTTONDOWN) {
        finishPick({});
        return 1;
    }

    return CallNextHookEx(g_state->mouseHook, code, wParam, lParam);
}

LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_state) {
        return CallNextHookEx(g_state ? g_state->keyboardHook : nullptr, code, wParam, lParam);
    }

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (info->vkCode == VK_ESCAPE) {
            finishPick({});
            return 1;
        }
    }

    return CallNextHookEx(g_state->keyboardHook, code, wParam, lParam);
}

} // namespace
#endif

void WindowPicker::startPick(QWidget* hostWidget, Completion callback) {
#ifdef _WIN32
    cancelPick();
    WindowPickerHoverOverlay::dismissAll();

    auto state = std::make_unique<PickState>();
    state->callback = std::move(callback);
    state->host = hostWidget;
    state->crossCursor = LoadCursorW(nullptr, IDC_CROSS);

    state->mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, GetModuleHandleW(nullptr), 0);
    state->keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandleW(nullptr), 0);
    if (!state->mouseHook || !state->keyboardHook) {
        teardownHooks(*state);
        WindowPicker::Result failed;
        if (callback) {
            QTimer::singleShot(0, qApp, [callback, failed]() { callback(failed); });
        }
        return;
    }

    g_state = std::move(state);
#else
    (void)hostWidget;
    if (callback) {
        callback({});
    }
#endif
}

void WindowPicker::cancelPick() {
#ifdef _WIN32
    if (g_state) {
        finishPick({});
    }
#endif
}
