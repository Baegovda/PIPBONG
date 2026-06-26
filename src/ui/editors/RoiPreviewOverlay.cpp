#include "ui/editors/MatchTestOverlay.h"
#include "ui/editors/RoiPreviewOverlay.h"

#include "core/capture/ScreenCapture.h"

#include <QApplication>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <memory>
#include <string>

namespace {

#ifdef _WIN32
constexpr wchar_t kPreviewClassName[] = L"SbmRoiPreviewOverlay";
constexpr int kEscHotkeyId = 0x5052;

struct PreviewState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    std::vector<QRect> roiClientRects;
    std::wstring hintText;
    RoiPreviewOverlay::VisibilityHandler onVisibilityChanged;
};

std::unique_ptr<PreviewState> g_state;

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

void notifyHidden() {
    RoiPreviewOverlay::VisibilityHandler callback;
    if (g_state) {
        callback = std::move(g_state->onVisibilityChanged);
    }
    if (callback) {
        QTimer::singleShot(0, qApp, [callback = std::move(callback)]() { callback(false); });
    }
}

void destroyPreviewWindow() {
    if (!g_state || !g_state->hwnd) {
        g_state.reset();
        return;
    }

    UnregisterHotKey(g_state->hwnd, kEscHotkeyId);
    DestroyWindow(g_state->hwnd);
    g_state->hwnd = nullptr;
    notifyHidden();
    g_state.reset();
}

QRect roiClientRectOnTargetWindow(const QRect& roiPhysical, const QRect& targetPhysical) {
    QRect roiClient = roiPhysical.translated(-targetPhysical.x(), -targetPhysical.y());
    const QRect targetClient(0, 0, targetPhysical.width(), targetPhysical.height());
    return roiClient.intersected(targetClient);
}

void paintPreview(HDC hdc, const PreviewState& state) {
    RECT clientRect{};
    GetClientRect(state.hwnd, &clientRect);

    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &clientRect, dimBrush);
    DeleteObject(dimBrush);

    for (const QRect& roiRect : state.roiClientRects) {
        if (roiRect.width() < 2 || roiRect.height() < 2) {
            continue;
        }
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(80, 255, 120));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, roiRect.left(), roiRect.top(), roiRect.right() + 1, roiRect.bottom() + 1);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        HBRUSH fillBrush = CreateSolidBrush(RGB(40, 180, 90));
        const RECT inner = {roiRect.left() + 2,
                            roiRect.top() + 2,
                            roiRect.right() - 1,
                            roiRect.bottom() - 1};
        FillRect(hdc, &inner, fillBrush);
        DeleteObject(fillBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        const std::wstring sizeText =
            std::to_wstring(roiRect.width()) + L" x " + std::to_wstring(roiRect.height());
        TextOutW(hdc, roiRect.left() + 8, roiRect.top() + 8, sizeText.c_str(), static_cast<int>(sizeText.size()));
    }

    if (!state.hintText.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect{16, 16, clientRect.right - 16, clientRect.bottom - 16};
        DrawTextW(hdc, state.hintText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

LRESULT CALLBACK previewWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PreviewState* state = reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        state = static_cast<PreviewState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        SetLayeredWindowAttributes(hwnd, 0, 120, LWA_ALPHA);
        return 0;
    }
    case WM_PAINT: {
        if (!state) {
            break;
        }
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        paintPreview(hdc, *state);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == kEscHotkeyId) {
            RoiPreviewOverlay::dismissAll();
            return 0;
        }
        break;
    case WM_DESTROY:
        UnregisterHotKey(hwnd, kEscHotkeyId);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensurePreviewClassRegistered() {
    static const ATOM atom = []() -> ATOM {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = previewWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kPreviewClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        return RegisterClassExW(&wc);
    }();
    return atom != 0;
}

bool createPreviewOverlay(PreviewState& state) {
    if (!ensurePreviewClassRegistered()) {
        return false;
    }

    const int x = state.physicalBounds.x();
    const int y = state.physicalBounds.y();
    const int w = state.physicalBounds.width();
    const int h = state.physicalBounds.height();

    state.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                 kPreviewClassName,
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
    RegisterHotKey(state.hwnd, kEscHotkeyId, 0, VK_ESCAPE);
    InvalidateRect(state.hwnd, nullptr, TRUE);
    return true;
}

#endif // _WIN32

} // namespace

bool RoiPreviewOverlay::isVisible() {
#ifdef _WIN32
    return g_state && g_state->hwnd;
#else
    return false;
#endif
}

bool RoiPreviewOverlay::show(SearchArea searchArea,
                             const CaptureRegion& customRegion,
                             const PercentRegion& percentRegion,
                             const std::vector<CaptureRegion>& customRegions,
                             QWidget* hostWidget,
                             VisibilityHandler onVisibilityChanged) {
#ifdef _WIN32
    MatchTestOverlay::dismissAll();
    dismissAll();

    if (!ScreenCapture::findTargetWindow()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("대상 창을 찾을 수 없습니다.\n"
                                         "메인 창에서 먼저 '창 지정'을 사용하세요."));
        return false;
    }

    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("대상 창 영역을 확인할 수 없습니다."));
        return false;
    }

    const QRect targetPhysical(target.x, target.y, target.width, target.height);
    std::vector<QRect> roiClientRects;

    auto appendRoiClient = [&](const CaptureRegion& region) {
        QRect roiPhysical;
        if (!ScreenCapture::searchAreaPhysicalRect(
                SearchArea::CustomRegion, region, percentRegion, roiPhysical)) {
            return;
        }
        const QRect roiClient = roiClientRectOnTargetWindow(roiPhysical, targetPhysical);
        if (roiClient.width() >= 2 && roiClient.height() >= 2) {
            roiClientRects.push_back(roiClient);
        }
    };

    if (!customRegions.empty()) {
        for (const CaptureRegion& region : customRegions) {
            if (region.width >= 2 && region.height >= 2) {
                appendRoiClient(region);
            }
        }
    } else {
        QRect roiPhysical;
        if (!ScreenCapture::searchAreaPhysicalRect(searchArea, customRegion, percentRegion, roiPhysical)) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("ROI 미리보기"),
                                 QObject::tr("탐색 ROI를 확인할 수 없습니다."));
            return false;
        }
        const QRect roiClient = roiClientRectOnTargetWindow(roiPhysical, targetPhysical);
        if (roiClient.width() < 2 || roiClient.height() < 2) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("ROI 미리보기"),
                                 QObject::tr("탐색 ROI가 대상 창과 겹치지 않습니다."));
            return false;
        }
        roiClientRects.push_back(roiClient);
    }

    if (roiClientRects.empty()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("탐색 ROI가 대상 창과 겹치지 않습니다."));
        return false;
    }

    g_state = std::make_unique<PreviewState>();
    g_state->physicalBounds = targetPhysical;
    g_state->roiClientRects = std::move(roiClientRects);
    g_state->onVisibilityChanged = std::move(onVisibilityChanged);
    g_state->hintText =
        QObject::tr("탐색 ROI 미리보기 (클릭 통과)\nROI 미리보기 끄기 또는 Esc로 닫기").toStdWString();

    if (!createPreviewOverlay(*g_state)) {
        g_state.reset();
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("ROI 미리보기 오버레이를 표시하지 못했습니다."));
        return false;
    }

    return true;
#else
    (void)searchArea;
    (void)customRegion;
    (void)percentRegion;
    (void)customRegions;
    (void)hostWidget;
    (void)onVisibilityChanged;
    return false;
#endif
}

bool RoiPreviewOverlay::hide() {
#ifdef _WIN32
    if (!isVisible()) {
        return false;
    }
    dismissAll();
    return true;
#else
    return false;
#endif
}

bool RoiPreviewOverlay::toggle(SearchArea searchArea,
                               const CaptureRegion& customRegion,
                               const PercentRegion& percentRegion,
                               const std::vector<CaptureRegion>& customRegions,
                               QWidget* hostWidget,
                               VisibilityHandler onVisibilityChanged) {
    if (isVisible()) {
        hide();
        return false;
    }
    return show(searchArea, customRegion, percentRegion, customRegions, hostWidget, std::move(onVisibilityChanged));
}

void RoiPreviewOverlay::dismissAll() {
#ifdef _WIN32
    destroyPreviewWindow();
#endif
}
