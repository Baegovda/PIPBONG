#include "core/capture/ClickContinuousInputRecorder.h"

#include "core/capture/ScreenCapture.h"
#include "core/workflow/blocks/ClickBlock.h"

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

struct SessionState {
    QPointer<QWidget> host;
    bool useClientCoordinates = true;
    ClickContinuousInputRecorder::BlockFactory factory;
    ClickContinuousInputRecorder::ClickCallback onCaptured;
    ClickContinuousInputRecorder::ArmedCallback onArmedChanged;
    bool armed = false;
    HHOOK mouseHook = nullptr;
};

std::unique_ptr<SessionState> g_session;

void uninstallMouseHook(SessionState& state) {
    if (state.mouseHook) {
        UnhookWindowsHookEx(state.mouseHook);
        state.mouseHook = nullptr;
    }
}

void capturePointAt(const POINT& pt) {
    if (!g_session || !g_session->factory) {
        return;
    }

    int x = pt.x;
    int y = pt.y;
    if (g_session->useClientCoordinates) {
        HWND hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            POINT clientPt = pt;
            if (ScreenToClient(hwnd, &clientPt)) {
                x = clientPt.x;
                y = clientPt.y;
            }
        }
    }

    std::unique_ptr<ClickBlock> block = g_session->factory(x, y);
    if (!block || !g_session->onCaptured) {
        return;
    }

    ClickContinuousInputRecorder::ClickCallback callback = g_session->onCaptured;
    QTimer::singleShot(0, qApp, [callback = std::move(callback), block = std::move(block)]() mutable {
        if (callback && block) {
            callback(std::move(block));
        }
    });
}

LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_session || !g_session->armed) {
        return CallNextHookEx(g_session ? g_session->mouseHook : nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    if (info->flags & LLMHF_INJECTED) {
        return CallNextHookEx(g_session->mouseHook, code, wParam, lParam);
    }

    switch (wParam) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        capturePointAt(info->pt);
        break;
    default:
        break;
    }

    return CallNextHookEx(g_session->mouseHook, code, wParam, lParam);
}

void installMouseHook(SessionState& state) {
    if (state.mouseHook) {
        return;
    }
    state.mouseHook =
        SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, GetModuleHandleW(nullptr), 0);
}

void notifyArmedChanged(bool armed) {
    if (!g_session || !g_session->onArmedChanged) {
        return;
    }
    ClickContinuousInputRecorder::ArmedCallback callback = g_session->onArmedChanged;
    QTimer::singleShot(0, qApp, [callback = std::move(callback), armed]() {
        if (callback) {
            callback(armed);
        }
    });
}

} // namespace
#endif

void ClickContinuousInputRecorder::beginSession(QWidget* hostWidget,
                                                bool useClientCoordinates,
                                                BlockFactory factory,
                                                ClickCallback onCaptured,
                                                ArmedCallback onArmedChanged) {
#ifdef _WIN32
    endSession();

    auto state = std::make_unique<SessionState>();
    state->host = hostWidget ? hostWidget->window() : nullptr;
    state->useClientCoordinates = useClientCoordinates;
    state->factory = std::move(factory);
    state->onCaptured = std::move(onCaptured);
    state->onArmedChanged = std::move(onArmedChanged);
    g_session = std::move(state);
#else
    Q_UNUSED(hostWidget);
    Q_UNUSED(useClientCoordinates);
    Q_UNUSED(factory);
    Q_UNUSED(onCaptured);
    Q_UNUSED(onArmedChanged);
#endif
}

void ClickContinuousInputRecorder::endSession() {
#ifdef _WIN32
    if (!g_session) {
        return;
    }
    if (g_session->armed) {
        g_session->armed = false;
        notifyArmedChanged(false);
    }
    uninstallMouseHook(*g_session);
    g_session.reset();
#endif
}

bool ClickContinuousInputRecorder::isSessionActive() {
#ifdef _WIN32
    return static_cast<bool>(g_session);
#else
    return false;
#endif
}

bool ClickContinuousInputRecorder::isArmed() {
#ifdef _WIN32
    return g_session && g_session->armed;
#else
    return false;
#endif
}

void ClickContinuousInputRecorder::setArmed(bool armed) {
#ifdef _WIN32
    if (!g_session || g_session->armed == armed) {
        return;
    }
    g_session->armed = armed;
    if (armed) {
        installMouseHook(*g_session);
    } else {
        uninstallMouseHook(*g_session);
    }
    notifyArmedChanged(armed);
#else
    Q_UNUSED(armed);
#endif
}

bool ClickContinuousInputRecorder::isHostActive(QWidget* hostWidget) {
#ifdef _WIN32
    if (!g_session || !g_session->host || !hostWidget) {
        return false;
    }
    QWidget* hostWindow = hostWidget->window();
    return hostWindow && hostWindow == g_session->host && hostWindow->isVisible();
#else
    Q_UNUSED(hostWidget);
    return false;
#endif
}
