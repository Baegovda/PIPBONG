#include "ui/editors/MatchTestOverlay.h"
#include "ui/editors/RoiPreviewOverlay.h"
#include "ui/editors/ScreenRegionOverlay.h"

#include "core/capture/ScreenCapture.h"
#include "core/diagnostics/CrashReporter.h"

#include <QApplication>
#include <QEventLoop>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#endif

#include <algorithm>
#include <memory>
#include <string>

namespace {

#ifdef _WIN32
constexpr wchar_t kOverlayClassName[] = L"PipbongScreenRegionOverlay";
constexpr int kEscHotkeyId = 0x504F;
constexpr UINT kMsgCancelPick = WM_APP + 0x504F;

struct OverlayState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    std::vector<QRect> existingRoiClientRects;
    bool dragging = false;
    POINT originPhysical{};
    POINT currentPhysical{};
    std::wstring hintText;
    ScreenRegionOverlay::PickCompletion onComplete;
};

std::unique_ptr<OverlayState> g_state;
HHOOK g_keyboardHook = nullptr;

void cancelPick();
void uninstallKeyboardHook();

LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (info->vkCode == VK_ESCAPE && g_state && g_state->hwnd) {
            PostMessageW(g_state->hwnd, kMsgCancelPick, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

void installKeyboardHook() {
    if (!g_keyboardHook) {
        g_keyboardHook =
            SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandleW(nullptr), 0);
    }
}

void uninstallKeyboardHook() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
}

QPointer<QWidget> g_hostWidget;
QRect g_hostSavedGeometry;
bool g_hostParkedOffScreen = false;
bool g_hostShouldPark = true;
std::function<void()> g_deferredAfterHostRestore;

QWidget* messageBoxParent(QWidget* parent) {
    if (!parent) {
        return nullptr;
    }
    if (parent->isVisible()) {
        return parent;
    }
    if (QWidget* window = parent->window()) {
        return window;
    }
    return parent;
}

void parkHostOffScreen(QWidget* host) {
    if (!host) {
        return;
    }
    g_hostSavedGeometry = host->frameGeometry();
    g_hostParkedOffScreen = true;
    host->move(-g_hostSavedGeometry.width() - 1000, -g_hostSavedGeometry.height() - 1000);
}

void restoreHost(QWidget* host) {
    if (!host) {
        return;
    }
    if (g_hostParkedOffScreen) {
        host->setGeometry(g_hostSavedGeometry);
        g_hostParkedOffScreen = false;
    }
    host->raise();
    host->activateWindow();
}

void destroyOverlayWindow() {
    if (!g_state || !g_state->hwnd) {
        g_state.reset();
        return;
    }

    UnregisterHotKey(g_state->hwnd, kEscHotkeyId);
    if (GetCapture() == g_state->hwnd) {
        ReleaseCapture();
    }
    DestroyWindow(g_state->hwnd);
    g_state->hwnd = nullptr;
    uninstallKeyboardHook();
    g_state.reset();
}

void finishPick(bool accepted, const QRect& screenRect) {
    ScreenRegionOverlay::PickCompletion callback;
    const bool shouldRestoreHost = g_hostShouldPark;
    if (g_state) {
        callback = std::move(g_state->onComplete);
    }

    QPointer<QWidget> host = g_hostWidget;
    destroyOverlayWindow();
    g_hostWidget = nullptr;
    g_hostShouldPark = true;

    // BitBlt capture in the pick callback must run synchronously right after overlay teardown.
    // Modal UI must wait until the parked host is restored (deferUntilHostRestored).
    if (callback) {
        callback(accepted, screenRect);
    }

    QTimer::singleShot(0, qApp, [host, shouldRestoreHost]() {
        if (shouldRestoreHost) {
            restoreHost(host.data());
        }
        if (g_deferredAfterHostRestore) {
            auto action = std::move(g_deferredAfterHostRestore);
            g_deferredAfterHostRestore = {};
            action();
        }
    });
}

POINT clampToBounds(const OverlayState& state, POINT physical) {
    physical.x = std::clamp(physical.x,
                            static_cast<LONG>(state.physicalBounds.left()),
                            static_cast<LONG>(state.physicalBounds.right()));
    physical.y = std::clamp(physical.y,
                            static_cast<LONG>(state.physicalBounds.top()),
                            static_cast<LONG>(state.physicalBounds.bottom()));
    return physical;
}

QRect normalizePhysicalRect(POINT a, POINT b) {
    const int left = std::min(a.x, b.x);
    const int top = std::min(a.y, b.y);
    const int right = std::max(a.x, b.x);
    const int bottom = std::max(a.y, b.y);
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QRect selectionClientRect(const OverlayState& state) {
    const QRect selection = normalizePhysicalRect(state.originPhysical, state.currentPhysical);
    return QRect(selection.left() - state.physicalBounds.x(),
                 selection.top() - state.physicalBounds.y(),
                 selection.width(),
                 selection.height());
}

QRect roiClientRectOnTargetWindow(const QRect& roiPhysical, const QRect& targetPhysical) {
    QRect roiClient = roiPhysical.translated(-targetPhysical.x(), -targetPhysical.y());
    const QRect targetClient(0, 0, targetPhysical.width(), targetPhysical.height());
    return roiClient.intersected(targetClient);
}

void cancelPick() {
    finishPick(false, {});
}

void acceptPick(const OverlayState& state) {
    const QRect selection = normalizePhysicalRect(state.originPhysical, state.currentPhysical);
    if (selection.width() < 2 || selection.height() < 2) {
        return;
    }
    finishPick(true, selection);
}

void paintOverlay(HDC hdc, const OverlayState& state) {
    RECT clientRect{};
    GetClientRect(state.hwnd, &clientRect);

    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &clientRect, dimBrush);
    DeleteObject(dimBrush);

    for (const QRect& existingRoiRect : state.existingRoiClientRects) {
        if (existingRoiRect.width() < 2 || existingRoiRect.height() < 2) {
            continue;
        }
        HPEN existingPen = CreatePen(PS_DOT, 1, RGB(130, 135, 145));
        HGDIOBJ oldPen = SelectObject(hdc, existingPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc,
                  existingRoiRect.left(),
                  existingRoiRect.top(),
                  existingRoiRect.right() + 1,
                  existingRoiRect.bottom() + 1);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(existingPen);

        HBRUSH existingFill = CreateSolidBrush(RGB(52, 54, 62));
        const RECT existingInner = {existingRoiRect.left() + 1,
                                    existingRoiRect.top() + 1,
                                    existingRoiRect.right(),
                                    existingRoiRect.bottom()};
        FillRect(hdc, &existingInner, existingFill);
        DeleteObject(existingFill);
    }

    if (state.dragging) {
        const QRect sel = selectionClientRect(state);
        if (sel.width() >= 2 && sel.height() >= 2) {
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 180, 255));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HBRUSH fill = CreateSolidBrush(RGB(0, 180, 255));
            HGDIOBJ oldBrush = SelectObject(hdc, fill);

            Rectangle(hdc, sel.left(), sel.top(), sel.right() + 1, sel.bottom() + 1);

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(fill);
            DeleteObject(pen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            const std::wstring sizeText = std::to_wstring(sel.width()) + L" x " + std::to_wstring(sel.height());
            TextOutW(hdc, sel.left() + 6, sel.top() + 6, sizeText.c_str(), static_cast<int>(sizeText.size()));
        }
    }

    if (!state.hintText.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect{16, 16, clientRect.right - 16, clientRect.bottom - 16};
        DrawTextW(hdc, state.hintText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayState* state = reinterpret_cast<OverlayState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        state = static_cast<OverlayState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        SetLayeredWindowAttributes(hwnd, 0, 110, LWA_ALPHA);
        return 0;
    }
    case WM_PAINT: {
        if (!state) {
            break;
        }
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        paintOverlay(hdc, *state);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (state) {
            state->dragging = true;
            SetCapture(hwnd);
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(hwnd, &pt);
            state->originPhysical = clampToBounds(*state, pt);
            state->currentPhysical = state->originPhysical;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(hwnd, &pt);
            state->currentPhysical = clampToBounds(*state, pt);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state && state->dragging) {
            state->dragging = false;
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(hwnd, &pt);
            state->currentPhysical = clampToBounds(*state, pt);
            acceptPick(*state);
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            cancelPick();
            return 0;
        }
        if (wParam == VK_RETURN) {
            if (state) {
                acceptPick(*state);
            }
            return 0;
        }
        break;
    case WM_HOTKEY:
        if (wParam == kEscHotkeyId) {
            cancelPick();
            return 0;
        }
        break;
    case kMsgCancelPick:
        cancelPick();
        return 0;
    case WM_DESTROY:
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        UnregisterHotKey(hwnd, kEscHotkeyId);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensureOverlayClassRegistered() {
    static const ATOM atom = []() -> ATOM {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = overlayWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kOverlayClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        wc.hbrBackground = nullptr;
        return RegisterClassExW(&wc);
    }();
    return atom != 0;
}

bool createNativeOverlay(OverlayState& state) {
    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    const int x = state.physicalBounds.x();
    const int y = state.physicalBounds.y();
    const int w = state.physicalBounds.width();
    const int h = state.physicalBounds.height();

    state.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                                 kOverlayClassName,
                                 L"",
                                 WS_POPUP,
                                 x,
                                 y,
                                 w,
                                 h,
                                 nullptr,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 &state);

    if (!state.hwnd) {
        return false;
    }

    ShowWindow(state.hwnd, SW_SHOW);
    SetWindowPos(state.hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    if (!RegisterHotKey(state.hwnd, kEscHotkeyId, 0, VK_ESCAPE)) {
        DestroyWindow(state.hwnd);
        state.hwnd = nullptr;
        uninstallKeyboardHook();
        return false;
    }
    installKeyboardHook();
    SetForegroundWindow(state.hwnd);
    SetFocus(state.hwnd);
    InvalidateRect(state.hwnd, nullptr, TRUE);
    return true;
}

#endif // _WIN32

} // namespace

void ScreenRegionOverlay::dismissAll() {
#ifdef _WIN32
    g_deferredAfterHostRestore = {};
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
    if (g_state) {
        finishPick(false, {});
        return;
    }
    uninstallKeyboardHook();
    if (g_hostWidget) {
        restoreHost(g_hostWidget.data());
        g_hostWidget = nullptr;
    } else if (g_hostParkedOffScreen) {
        g_hostParkedOffScreen = false;
    }
#endif
}

bool ScreenRegionOverlay::isPickActive() {
#ifdef _WIN32
    return g_state != nullptr;
#else
    return false;
#endif
}

void ScreenRegionOverlay::deferUntilHostRestored(std::function<void()> action) {
    g_deferredAfterHostRestore = std::move(action);
}

void ScreenRegionOverlay::startPick(QWidget* hostWidget,
                                    PickCompletion onComplete,
                                    StartPickOptions options) {
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
    dismissAll();

    CrashReporter::noteBreadcrumb(QStringLiteral("overlay"),
                                  QStringLiteral("screen region pick start parkHost=%1")
                                      .arg(options.parkHostDuringPick ? QStringLiteral("yes")
                                                                      : QStringLiteral("no")));

#ifdef _WIN32
    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("템플릿 캡처"),
                             QObject::tr("타겟을 찾을 수 없습니다.\n"
                                         "메인 창에서 '타겟 지정' 또는 '창 목록'의 '서브 지정'을 사용하세요."));
        if (onComplete) {
            onComplete(false, {});
        }
        return;
    }

    g_hostShouldPark = options.parkHostDuringPick;
    g_hostWidget = hostWidget ? hostWidget->window() : nullptr;
    if (g_hostShouldPark && g_hostWidget) {
        parkHostOffScreen(g_hostWidget.data());
    }
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    g_state = std::make_unique<OverlayState>();
    g_state->physicalBounds = QRect(target.x, target.y, target.width, target.height);
    std::vector<QRect> existingPhysical = options.existingRoiPhysicalRects;
    if (options.existingRoiPhysical.width() >= 2 && options.existingRoiPhysical.height() >= 2) {
        existingPhysical.push_back(options.existingRoiPhysical);
    }
    for (const QRect& roiPhysical : existingPhysical) {
        if (roiPhysical.width() < 2 || roiPhysical.height() < 2) {
            continue;
        }
        const QRect roiClient = roiClientRectOnTargetWindow(roiPhysical, g_state->physicalBounds);
        if (roiClient.width() >= 2 && roiClient.height() >= 2) {
            g_state->existingRoiClientRects.push_back(roiClient);
        }
    }
    g_state->hintText = QObject::tr("타겟에서 드래그하여 영역을 선택하세요.\nEsc = 취소")
                            .toStdWString();
    g_state->onComplete = std::move(onComplete);

    if (!createNativeOverlay(*g_state)) {
        ScreenRegionOverlay::PickCompletion failedCallback = std::move(g_state->onComplete);
        g_state.reset();
        if (g_hostShouldPark) {
            restoreHost(g_hostWidget.data());
        }
        g_hostWidget = nullptr;
        g_hostShouldPark = true;
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("템플릿 캡처"),
                             QObject::tr("캡처 오버레이를 표시하지 못했습니다."));
        if (failedCallback) {
            failedCallback(false, {});
        }
        return;
    }
#else
    if (onComplete) {
        onComplete(false, {});
    }
#endif
}
