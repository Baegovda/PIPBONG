#include "core/capture/CursorPositionPicker.h"

#include "core/capture/ScreenCapture.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>

#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>

namespace {

#ifdef _WIN32
struct PickState {
    QPointer<QWidget> host;
    CursorPositionPicker::Completion callback;
    bool useClientCoordinates = true;
    HHOOK mouseHook = nullptr;
    HHOOK keyboardHook = nullptr;
    HCURSOR crossCursor = nullptr;
    QTimer* countdownTimer = nullptr;
    QLabel* hintWidget = nullptr;
    int secondsLeft = 3;
};

std::unique_ptr<PickState> g_state;

void destroyHint(PickState& state) {
    QLabel* hint = state.hintWidget;
    if (!hint) {
        return;
    }
    state.hintWidget = nullptr;
    hint->hide();
    QTimer::singleShot(0, qApp, [hint]() { hint->deleteLater(); });
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
    if (state.countdownTimer) {
        state.countdownTimer->stop();
        state.countdownTimer->deleteLater();
        state.countdownTimer = nullptr;
    }
    destroyHint(state);
}

void finishPick(CursorPositionPicker::Result result) {
    if (!g_state) {
        return;
    }

    QWidget* host = g_state->host.data();
    CursorPositionPicker::Completion callback = std::move(g_state->callback);
    teardownHooks(*g_state);
    g_state.reset();
    (void)host;

    QTimer::singleShot(0, qApp, [callback = std::move(callback), result = std::move(result)]() mutable {
        if (callback) {
            callback(result);
        }
    });
}

constexpr int kHintOffsetX = 18;
constexpr int kHintOffsetY = -6;

void positionHintAtCursor(PickState& state, int cursorX, int cursorY) {
    if (!state.hintWidget) {
        return;
    }

    state.hintWidget->adjustSize();
    const QSize hintSize = state.hintWidget->size();

    int x = cursorX + kHintOffsetX;
    int y = cursorY + kHintOffsetY;

    QScreen* screen = QGuiApplication::screenAt(QPoint(cursorX, cursorY));
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        state.hintWidget->move(x, y);
        return;
    }

    const QRect avail = screen->availableGeometry();
    if (x + hintSize.width() > avail.right()) {
        x = cursorX - hintSize.width() - kHintOffsetX;
    }
    if (y + hintSize.height() > avail.bottom()) {
        y = avail.bottom() - hintSize.height();
    }
    y = std::max(y, avail.top());
    x = std::max(x, avail.left());

    state.hintWidget->move(x, y);
}

void updateHintText(PickState& state) {
    if (!state.hintWidget) {
        return;
    }
    state.hintWidget->setText(
        QCoreApplication::translate("CursorPositionPicker",
                                    "%1초 후 현재 마우스 위치를 기록합니다… (Esc 취소)")
            .arg(state.secondsLeft));
    POINT pt{};
    if (GetCursorPos(&pt)) {
        positionHintAtCursor(state, pt.x, pt.y);
    }
}

void showHint(PickState& state) {
    if (state.hintWidget) {
        updateHintText(state);
        return;
    }

    auto* hint = new QLabel();
    hint->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    hint->setStyleSheet(QStringLiteral(
        "background-color: rgba(20, 20, 20, 220); color: white; padding: 10px 16px; border-radius: 6px;"));
    hint->setAttribute(Qt::WA_DeleteOnClose);
    state.hintWidget = hint;
    updateHintText(state);
    hint->show();
}

void captureCursorPosition(PickState& state, CursorPositionPicker::Result& result) {
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return;
    }

    if (state.useClientCoordinates) {
        HWND hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            POINT clientPt = pt;
            if (ScreenToClient(hwnd, &clientPt)) {
                result.x = clientPt.x;
                result.y = clientPt.y;
                result.accepted = true;
                return;
            }
        }
    }

    result.x = pt.x;
    result.y = pt.y;
    result.accepted = true;
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
        if (g_state->hintWidget) {
            positionHintAtCursor(*g_state, info->pt.x, info->pt.y);
        }
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

void onCountdownTick() {
    if (!g_state || !g_state->countdownTimer) {
        return;
    }

    --g_state->secondsLeft;
    if (g_state->secondsLeft <= 0) {
        CursorPositionPicker::Result result;
        captureCursorPosition(*g_state, result);
        finishPick(result);
        return;
    }

    updateHintText(*g_state);
}

#endif

} // namespace

void CursorPositionPicker::startPick(QWidget* hostWidget,
                                     bool useClientCoordinates,
                                     int delaySeconds,
                                     Completion callback) {
#ifdef _WIN32
    cancelPick();

    if (delaySeconds < 1) {
        delaySeconds = 1;
    }

    auto state = std::make_unique<PickState>();
    state->host = hostWidget ? hostWidget->window() : nullptr;
    state->callback = std::move(callback);
    state->useClientCoordinates = useClientCoordinates;
    state->secondsLeft = delaySeconds;
    state->crossCursor = LoadCursorW(nullptr, IDC_CROSS);

    state->mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, GetModuleHandleW(nullptr), 0);
    state->keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandleW(nullptr), 0);
    if (!state->mouseHook || !state->keyboardHook) {
        destroyHint(*state);
        teardownHooks(*state);
        if (callback) {
            QTimer::singleShot(0, qApp, [callback]() { callback({}); });
        }
        return;
    }

    showHint(*state);

    state->countdownTimer = new QTimer(qApp);
    state->countdownTimer->setInterval(1000);
    QObject::connect(state->countdownTimer, &QTimer::timeout, qApp, []() { onCountdownTick(); });
    state->countdownTimer->start();

    g_state = std::move(state);
#else
    (void)hostWidget;
    (void)useClientCoordinates;
    (void)delaySeconds;
    if (callback) {
        callback({});
    }
#endif
}

void CursorPositionPicker::cancelPick() {
#ifdef _WIN32
    if (g_state) {
        finishPick({});
    }
#endif
}

bool CursorPositionPicker::isActive() {
#ifdef _WIN32
    return static_cast<bool>(g_state);
#else
    return false;
#endif
}
