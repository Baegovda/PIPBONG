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
    struct RoiItem {
        QRect clientRect;
        int index = 1;
    };
    std::vector<RoiItem> roiItems;
    int selectedRoiIndex = 0;
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

void drawRoiIndexBadge(HDC hdc, const QRect& roiRect, int roiIndex, bool selected) {
    wchar_t label[16]{};
    swprintf_s(label, L"#%d", roiIndex);
    const int labelLen = static_cast<int>(wcslen(label));

    HFONT font = CreateFontW(-22,
                             0,
                             0,
                             0,
                             FW_BOLD,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, font);

    SIZE textSize{};
    GetTextExtentPoint32W(hdc, label, labelLen, &textSize);

    constexpr int kPadX = 6;
    constexpr int kPadY = 4;
    int textX = roiRect.left() + 6;
    int textY = roiRect.top() + 6;
    if (textX + textSize.cx + kPadX * 2 > roiRect.right()) {
        textX = std::max(roiRect.left() + 2, roiRect.right() - textSize.cx - kPadX * 2 - 2);
    }
    if (textY + textSize.cy + kPadY * 2 > roiRect.bottom()) {
        textY = std::max(roiRect.top() + 2, roiRect.bottom() - textSize.cy - kPadY * 2 - 2);
    }

    RECT bgRect{textX,
                textY,
                textX + textSize.cx + kPadX * 2,
                textY + textSize.cy + kPadY * 2};

    const COLORREF badgeBorder = selected ? RGB(255, 214, 90) : RGB(80, 255, 120);
    const COLORREF badgeBg = selected ? RGB(48, 38, 12) : RGB(24, 24, 24);
    const COLORREF badgeText = selected ? RGB(255, 244, 200) : RGB(255, 255, 255);

    HBRUSH bgBrush = CreateSolidBrush(badgeBg);
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, selected ? 2 : 1, badgeBorder);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, bgRect.left, bgRect.top, bgRect.right, bgRect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, badgeText);
    TextOutW(hdc, bgRect.left + kPadX, bgRect.top + kPadY, label, labelLen);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void paintPreview(HDC hdc, const PreviewState& state) {
    RECT clientRect{};
    GetClientRect(state.hwnd, &clientRect);

    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &clientRect, dimBrush);
    DeleteObject(dimBrush);

    for (const PreviewState::RoiItem& roiItem : state.roiItems) {
        const QRect& roiRect = roiItem.clientRect;
        if (roiRect.width() < 2 || roiRect.height() < 2) {
            continue;
        }

        const bool selected =
            state.selectedRoiIndex > 0 && roiItem.index == state.selectedRoiIndex;
        const COLORREF borderColor = selected ? RGB(255, 214, 90) : RGB(60, 150, 85);
        const COLORREF fillColor = selected ? RGB(70, 170, 95) : RGB(28, 72, 48);
        const int borderWidth = selected ? 4 : 2;

        if (selected) {
            HPEN haloPen = CreatePen(PS_SOLID, 6, RGB(255, 244, 170));
            HGDIOBJ oldHaloPen = SelectObject(hdc, haloPen);
            HGDIOBJ oldHaloBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            const int haloInset = 3;
            Rectangle(hdc,
                      roiRect.left() - haloInset,
                      roiRect.top() - haloInset,
                      roiRect.right() + haloInset + 1,
                      roiRect.bottom() + haloInset + 1);
            SelectObject(hdc, oldHaloBrush);
            SelectObject(hdc, oldHaloPen);
            DeleteObject(haloPen);
        }

        HPEN pen = CreatePen(PS_SOLID, borderWidth, borderColor);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, roiRect.left(), roiRect.top(), roiRect.right() + 1, roiRect.bottom() + 1);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        HBRUSH fillBrush = CreateSolidBrush(fillColor);
        const int fillInset = selected ? 3 : 2;
        const RECT inner = {roiRect.left() + fillInset,
                            roiRect.top() + fillInset,
                            roiRect.right() - fillInset + 1,
                            roiRect.bottom() - fillInset + 1};
        FillRect(hdc, &inner, fillBrush);
        DeleteObject(fillBrush);

        drawRoiIndexBadge(hdc, roiRect, roiItem.index, selected);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, selected ? RGB(255, 248, 220) : RGB(200, 230, 205));
        HFONT sizeFont = CreateFontW(-16,
                                     0,
                                     0,
                                     0,
                                     FW_NORMAL,
                                     FALSE,
                                     FALSE,
                                     FALSE,
                                     DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE,
                                     L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(hdc, sizeFont);
        const std::wstring sizeText =
            std::to_wstring(roiRect.width()) + L" x " + std::to_wstring(roiRect.height());
        const int sizeY = roiRect.top() + 38;
        if (sizeY + 16 <= roiRect.bottom()) {
            TextOutW(hdc, roiRect.left() + 8, sizeY, sizeText.c_str(), static_cast<int>(sizeText.size()));
        }
        SelectObject(hdc, oldFont);
        DeleteObject(sizeFont);
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
                             VisibilityHandler onVisibilityChanged,
                             int selectedRoiIndex) {
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
    std::vector<PreviewState::RoiItem> roiItems;

    auto appendRoiClient = [&](const CaptureRegion& region, int roiIndex) {
        QRect roiPhysical;
        if (!ScreenCapture::searchAreaPhysicalRect(
                SearchArea::CustomRegion, region, percentRegion, roiPhysical)) {
            return;
        }
        const QRect roiClient = roiClientRectOnTargetWindow(roiPhysical, targetPhysical);
        if (roiClient.width() >= 2 && roiClient.height() >= 2) {
            roiItems.push_back({roiClient, roiIndex});
        }
    };

    if (!customRegions.empty()) {
        int roiIndex = 1;
        for (const CaptureRegion& region : customRegions) {
            if (region.width >= 2 && region.height >= 2) {
                appendRoiClient(region, roiIndex++);
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
        roiItems.push_back({roiClient, 1});
    }

    if (roiItems.empty()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("탐색 ROI가 대상 창과 겹치지 않습니다."));
        return false;
    }

    g_state = std::make_unique<PreviewState>();
    g_state->physicalBounds = targetPhysical;
    g_state->roiItems = std::move(roiItems);
    g_state->selectedRoiIndex = std::max(0, selectedRoiIndex);
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
    (void)selectedRoiIndex;
    return false;
#endif
}

void RoiPreviewOverlay::setSelectedRoiIndex(int selectedRoiIndex) {
#ifdef _WIN32
    if (!g_state || !g_state->hwnd) {
        return;
    }
    const int normalized = std::max(0, selectedRoiIndex);
    if (g_state->selectedRoiIndex == normalized) {
        return;
    }
    g_state->selectedRoiIndex = normalized;
    InvalidateRect(g_state->hwnd, nullptr, TRUE);
#else
    (void)selectedRoiIndex;
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
