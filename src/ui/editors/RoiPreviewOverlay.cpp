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
#include <windowsx.h>
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

#ifdef _WIN32
constexpr wchar_t kPreviewClassName[] = L"SbmRoiPreviewOverlay";
constexpr int kEscHotkeyId = 0x5052;

constexpr int kToolbarHeight = 28;
constexpr int kToolbarGap = 4;
constexpr int kChromeSpacing = 4;
constexpr int kButtonPadX = 10;
constexpr int kHandleHitSize = 10;
constexpr int kMinRoiSize = 8;

enum class ResizeHandle {
    None,
    Move,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

struct PreviewState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    struct RoiItem {
        QRect clientRect;
        int index = 1;
    };
    std::vector<RoiItem> roiItems;
    int selectedRoiIndex = 0;
    bool interactive = false;
    std::wstring hintText;
    RoiPreviewOverlay::VisibilityHandler onVisibilityChanged;
    RoiPreviewOverlay::RoiIndexHandler onRoiSelected;
    RoiPreviewOverlay::RoiActionHandler onRoiAdd;
    RoiPreviewOverlay::RoiRegionHandler onRoiRegionChanged;
    RoiPreviewOverlay::RoiIndexHandler onRoiDelete;
    int hoveredRoiIndex = 0;
    enum class HoveredControl { None, Add, Delete };
    HoveredControl hoveredControl = HoveredControl::None;
    ResizeHandle hoveredResizeHandle = ResizeHandle::None;
    bool dragging = false;
    int dragRoiIndex = 0;
    ResizeHandle dragHandle = ResizeHandle::None;
    QPoint dragStartPoint;
    QRect dragStartRect;
};

struct RoiChromeLayout {
    int roiIndex = 0;
    QRect roiRect;
    QRect labelRect;
    QRect addRect;
    QRect deleteRect;
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

HFONT createUiFont(int heightPx, int weight) {
    return CreateFontW(heightPx,
                       0,
                       0,
                       0,
                       weight,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");
}

SIZE measureText(HDC hdc, const wchar_t* text, int length) {
    SIZE size{};
    if (length < 0) {
        length = static_cast<int>(wcslen(text));
    }
    GetTextExtentPoint32W(hdc, text, length, &size);
    return size;
}

RECT qRectToRect(const QRect& rect) {
    return {rect.left(), rect.top(), rect.right(), rect.bottom()};
}

bool pointInRect(const QPoint& point, const QRect& rect) {
    return rect.contains(point);
}

QRect targetClientBounds(const PreviewState& state) {
    return QRect(0, 0, state.physicalBounds.width(), state.physicalBounds.height());
}

PreviewState::RoiItem* findRoiItem(PreviewState& state, int roiIndex) {
    for (PreviewState::RoiItem& item : state.roiItems) {
        if (item.index == roiIndex) {
            return &item;
        }
    }
    return nullptr;
}

CaptureRegion clientRectToCaptureRegion(const PreviewState& state, const QRect& clientRect) {
    CaptureRegion region;
    region.x = state.physicalBounds.x() + clientRect.x();
    region.y = state.physicalBounds.y() + clientRect.y();
    region.width = clientRect.width();
    region.height = clientRect.height();
    return region;
}

QRect normalizeDragRect(QRect rect, const QRect& bounds) {
    rect = rect.normalized();
    if (rect.width() < kMinRoiSize) {
        rect.setWidth(kMinRoiSize);
    }
    if (rect.height() < kMinRoiSize) {
        rect.setHeight(kMinRoiSize);
    }
    if (rect.left() < bounds.left()) {
        rect.moveLeft(bounds.left());
    }
    if (rect.top() < bounds.top()) {
        rect.moveTop(bounds.top());
    }
    if (rect.right() > bounds.right()) {
        rect.moveRight(bounds.right());
    }
    if (rect.bottom() > bounds.bottom()) {
        rect.moveBottom(bounds.bottom());
    }
    if (rect.width() < kMinRoiSize || rect.height() < kMinRoiSize) {
        return {};
    }
    return rect;
}

QRect rectFromResize(const QRect& start, ResizeHandle handle, int dx, int dy, const QRect& bounds) {
    int left = start.left();
    int top = start.top();
    int right = start.right();
    int bottom = start.bottom();
    switch (handle) {
    case ResizeHandle::TopLeft:
        left += dx;
        top += dy;
        break;
    case ResizeHandle::Top:
        top += dy;
        break;
    case ResizeHandle::TopRight:
        right += dx;
        top += dy;
        break;
    case ResizeHandle::Right:
        right += dx;
        break;
    case ResizeHandle::BottomRight:
        right += dx;
        bottom += dy;
        break;
    case ResizeHandle::Bottom:
        bottom += dy;
        break;
    case ResizeHandle::BottomLeft:
        left += dx;
        bottom += dy;
        break;
    case ResizeHandle::Left:
        left += dx;
        break;
    case ResizeHandle::Move:
        return normalizeDragRect(start.translated(dx, dy), bounds);
    default:
        break;
    }
    return normalizeDragRect(QRect(QPoint(left, top), QPoint(right, bottom)), bounds);
}

QRect handleHitRect(const QPoint& center) {
    const int half = kHandleHitSize / 2;
    return QRect(center.x() - half, center.y() - half, kHandleHitSize, kHandleHitSize);
}

ResizeHandle hitTestResizeHandle(const QRect& roiRect, const QPoint& point, bool selectedOnly, bool selected) {
    if (selectedOnly && !selected) {
        return ResizeHandle::None;
    }
    const QPoint centers[] = {
        roiRect.topLeft(),
        QPoint(roiRect.center().x(), roiRect.top()),
        roiRect.topRight(),
        QPoint(roiRect.right(), roiRect.center().y()),
        roiRect.bottomRight(),
        QPoint(roiRect.center().x(), roiRect.bottom()),
        roiRect.bottomLeft(),
        QPoint(roiRect.left(), roiRect.center().y()),
    };
    const ResizeHandle handles[] = {
        ResizeHandle::TopLeft,
        ResizeHandle::Top,
        ResizeHandle::TopRight,
        ResizeHandle::Right,
        ResizeHandle::BottomRight,
        ResizeHandle::Bottom,
        ResizeHandle::BottomLeft,
        ResizeHandle::Left,
    };
    for (size_t i = 0; i < std::size(handles); ++i) {
        if (handleHitRect(centers[i]).contains(point)) {
            return handles[i];
        }
    }
    if (roiRect.contains(point)) {
        return ResizeHandle::Move;
    }
    return ResizeHandle::None;
}

void drawResizeHandles(HDC hdc, const QRect& roiRect) {
    const QPoint centers[] = {
        roiRect.topLeft(),
        QPoint(roiRect.center().x(), roiRect.top()),
        roiRect.topRight(),
        QPoint(roiRect.right(), roiRect.center().y()),
        roiRect.bottomRight(),
        QPoint(roiRect.center().x(), roiRect.bottom()),
        roiRect.bottomLeft(),
        QPoint(roiRect.left(), roiRect.center().y()),
    };
    HBRUSH handleBrush = CreateSolidBrush(RGB(255, 244, 200));
    HPEN handlePen = CreatePen(PS_SOLID, 1, RGB(255, 214, 90));
    HGDIOBJ oldBrush = SelectObject(hdc, handleBrush);
    HGDIOBJ oldPen = SelectObject(hdc, handlePen);
    for (const QPoint& center : centers) {
        const QRect handle = handleHitRect(center);
        Rectangle(hdc, handle.left(), handle.top(), handle.right() + 1, handle.bottom() + 1);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(handleBrush);
    DeleteObject(handlePen);
}

HCURSOR cursorForResizeHandle(ResizeHandle handle) {
    LPCWSTR resource = IDC_ARROW;
    switch (handle) {
    case ResizeHandle::TopLeft:
    case ResizeHandle::BottomRight:
        resource = IDC_SIZENWSE;
        break;
    case ResizeHandle::TopRight:
    case ResizeHandle::BottomLeft:
        resource = IDC_SIZENESW;
        break;
    case ResizeHandle::Top:
    case ResizeHandle::Bottom:
        resource = IDC_SIZENS;
        break;
    case ResizeHandle::Left:
    case ResizeHandle::Right:
        resource = IDC_SIZEWE;
        break;
    case ResizeHandle::Move:
        resource = IDC_SIZEALL;
        break;
    default:
        break;
    }
    return LoadCursorW(nullptr, resource);
}

void invokeRoiRegionHandler(RoiPreviewOverlay::RoiRegionHandler handler,
                            int roiIndex,
                            const CaptureRegion& region) {
    if (!handler) {
        return;
    }
    QTimer::singleShot(0, qApp, [handler = std::move(handler), roiIndex, region]() {
        handler(roiIndex, region);
    });
}

void drawToolbarButton(HDC hdc,
                       const QRect& rect,
                       const wchar_t* text,
                       bool hovered,
                       bool destructive) {
    const COLORREF face = hovered ? (destructive ? RGB(92, 38, 38) : RGB(68, 72, 78))
                                  : (destructive ? RGB(72, 32, 32) : RGB(52, 56, 60));
    const COLORREF border = hovered ? (destructive ? RGB(220, 90, 90) : RGB(150, 156, 166))
                                    : (destructive ? RGB(150, 70, 70) : RGB(110, 116, 126));
    const COLORREF textColor = RGB(245, 245, 245);

    RECT faceRect = qRectToRect(rect);
    HBRUSH faceBrush = CreateSolidBrush(face);
    FillRect(hdc, &faceRect, faceBrush);
    DeleteObject(faceBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rect.left(), rect.top(), rect.right(), rect.bottom());
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    const int textLen = static_cast<int>(wcslen(text));
    SIZE textSize = measureText(hdc, text, textLen);
    const int textX = rect.left() + (std::max)(0, (rect.width() - static_cast<int>(textSize.cx)) / 2);
    const int textY = rect.top() + (std::max)(0, (rect.height() - static_cast<int>(textSize.cy)) / 2);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    TextOutW(hdc, textX, textY, text, textLen);
}

void drawIndexLabel(HDC hdc, const QRect& rect, int roiIndex, bool selected) {
    wchar_t label[16]{};
    swprintf_s(label, L"#%d", roiIndex);

    const COLORREF badgeBorder = selected ? RGB(255, 214, 90) : RGB(80, 255, 120);
    const COLORREF badgeBg = selected ? RGB(48, 38, 12) : RGB(24, 24, 24);
    const COLORREF badgeText = selected ? RGB(255, 244, 200) : RGB(255, 255, 255);

    RECT labelBgRect = qRectToRect(rect);
    HBRUSH bgBrush = CreateSolidBrush(badgeBg);
    FillRect(hdc, &labelBgRect, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, selected ? 2 : 1, badgeBorder);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rect.left(), rect.top(), rect.right(), rect.bottom());
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    const int textLen = static_cast<int>(wcslen(label));
    SIZE textSize = measureText(hdc, label, textLen);
    const int textX = rect.left() + (std::max)(0, (rect.width() - static_cast<int>(textSize.cx)) / 2);
    const int textY = rect.top() + (std::max)(0, (rect.height() - static_cast<int>(textSize.cy)) / 2);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, badgeText);
    TextOutW(hdc, textX, textY, label, textLen);
}

std::vector<RoiChromeLayout> buildChromeLayouts(const PreviewState& state, HDC hdc) {
    std::vector<RoiChromeLayout> layouts;
    layouts.reserve(state.roiItems.size());

    HFONT buttonFont = createUiFont(-14, FW_NORMAL);
    HGDIOBJ oldFont = SelectObject(hdc, buttonFont);

    const SIZE addSize = measureText(hdc, L"\ucd94\uac00", 2);
    const SIZE deleteSize = measureText(hdc, L"\uc0ad\uc81c", 2);
    const int addWidth = addSize.cx + kButtonPadX * 2;
    const int deleteWidth = deleteSize.cx + kButtonPadX * 2;

    HFONT labelFont = createUiFont(-16, FW_BOLD);
    SelectObject(hdc, labelFont);

    for (const PreviewState::RoiItem& roiItem : state.roiItems) {
        if (roiItem.clientRect.width() < 2 || roiItem.clientRect.height() < 2) {
            continue;
        }

        RoiChromeLayout layout;
        layout.roiIndex = roiItem.index;
        layout.roiRect = roiItem.clientRect;

        wchar_t indexLabel[16]{};
        swprintf_s(indexLabel, L"#%d", roiItem.index);
        const SIZE labelSize = measureText(hdc, indexLabel, static_cast<int>(wcslen(indexLabel)));
        const int labelWidth = labelSize.cx + kButtonPadX * 2;

        const int toolbarY = std::max(4, layout.roiRect.top() - kToolbarHeight - kToolbarGap);
        int cursorX = layout.roiRect.left();

        layout.labelRect = QRect(cursorX, toolbarY, labelWidth, kToolbarHeight);
        cursorX = layout.labelRect.right() + kChromeSpacing;

        if (state.interactive) {
            layout.addRect = QRect(cursorX, toolbarY, addWidth, kToolbarHeight);
            cursorX = layout.addRect.right() + kChromeSpacing;
            layout.deleteRect = QRect(cursorX, toolbarY, deleteWidth, kToolbarHeight);
        }

        layouts.push_back(layout);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(buttonFont);
    DeleteObject(labelFont);
    return layouts;
}

void invokeRoiHandler(RoiPreviewOverlay::RoiIndexHandler handler, int roiIndex) {
    if (!handler) {
        return;
    }
    QTimer::singleShot(0, qApp, [handler = std::move(handler), roiIndex]() { handler(roiIndex); });
}

void invokeRoiAction(RoiPreviewOverlay::RoiActionHandler handler) {
    if (!handler) {
        return;
    }
    QTimer::singleShot(0, qApp, [handler = std::move(handler)]() { handler(); });
}

void paintPreview(HDC hdc, PreviewState& state) {
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
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            const int haloInset = 3;
            Rectangle(hdc,
                      roiRect.left() - haloInset,
                      roiRect.top() - haloInset,
                      roiRect.right() + haloInset + 1,
                      roiRect.bottom() + haloInset + 1);
            SelectObject(hdc, oldBrush);
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

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, selected ? RGB(255, 248, 220) : RGB(200, 230, 205));
        HFONT sizeFont = createUiFont(-16, FW_NORMAL);
        HGDIOBJ oldFont = SelectObject(hdc, sizeFont);
        const std::wstring sizeText =
            std::to_wstring(roiRect.width()) + L" x " + std::to_wstring(roiRect.height());
        const int sizeY = roiRect.bottom() - 22;
        if (sizeY > roiRect.top() + 8) {
            TextOutW(hdc, roiRect.left() + 8, sizeY, sizeText.c_str(), static_cast<int>(sizeText.size()));
        }
        SelectObject(hdc, oldFont);
        DeleteObject(sizeFont);

        if (selected && state.interactive) {
            drawResizeHandles(hdc, roiRect);
        }
    }

    const std::vector<RoiChromeLayout> chromeLayouts = buildChromeLayouts(state, hdc);
    HFONT buttonFont = createUiFont(-14, FW_NORMAL);
    HGDIOBJ oldButtonFont = SelectObject(hdc, buttonFont);
    HFONT labelFont = createUiFont(-16, FW_BOLD);
    HGDIOBJ oldLabelFont = SelectObject(hdc, labelFont);

    for (const RoiChromeLayout& layout : chromeLayouts) {
        const bool selected =
            state.selectedRoiIndex > 0 && layout.roiIndex == state.selectedRoiIndex;
        drawIndexLabel(hdc, layout.labelRect, layout.roiIndex, selected);

        if (state.interactive) {
            const bool addHovered = state.hoveredRoiIndex == layout.roiIndex
                                    && state.hoveredControl == PreviewState::HoveredControl::Add;
            const bool deleteHovered = state.hoveredRoiIndex == layout.roiIndex
                                       && state.hoveredControl == PreviewState::HoveredControl::Delete;
            SelectObject(hdc, buttonFont);
            drawToolbarButton(hdc, layout.addRect, L"\ucd94\uac00", addHovered, false);
            drawToolbarButton(hdc, layout.deleteRect, L"\uc0ad\uc81c", deleteHovered, true);
        }
    }

    SelectObject(hdc, oldLabelFont);
    SelectObject(hdc, oldButtonFont);
    DeleteObject(buttonFont);
    DeleteObject(labelFont);

    if (!state.hintText.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect{16, 16, clientRect.right - 16, clientRect.bottom - 16};
        DrawTextW(hdc, state.hintText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

bool hitTestInteractive(const PreviewState& state,
                        const QPoint& point,
                        HDC hdc,
                        int* outRoiIndex,
                        PreviewState::HoveredControl* outControl,
                        ResizeHandle* outResizeHandle) {
    const std::vector<RoiChromeLayout> layouts = buildChromeLayouts(state, hdc);
    for (const RoiChromeLayout& layout : layouts) {
        if (state.interactive) {
            if (pointInRect(point, layout.deleteRect)) {
                *outRoiIndex = layout.roiIndex;
                *outControl = PreviewState::HoveredControl::Delete;
                *outResizeHandle = ResizeHandle::None;
                return true;
            }
            if (pointInRect(point, layout.addRect)) {
                *outRoiIndex = layout.roiIndex;
                *outControl = PreviewState::HoveredControl::Add;
                *outResizeHandle = ResizeHandle::None;
                return true;
            }
        }
    }
    for (const RoiChromeLayout& layout : layouts) {
        const bool selected = state.selectedRoiIndex > 0 && layout.roiIndex == state.selectedRoiIndex;
        if (state.interactive) {
            const ResizeHandle handle =
                hitTestResizeHandle(layout.roiRect, point, true, selected);
            if (handle != ResizeHandle::None) {
                *outRoiIndex = layout.roiIndex;
                *outControl = PreviewState::HoveredControl::None;
                *outResizeHandle = handle;
                return true;
            }
        }
        if (pointInRect(point, layout.labelRect) || pointInRect(point, layout.roiRect)) {
            *outRoiIndex = layout.roiIndex;
            *outControl = PreviewState::HoveredControl::None;
            *outResizeHandle = ResizeHandle::None;
            return true;
        }
    }
    return false;
}

void updateHoverFromPoint(PreviewState& state, const QPoint& point) {
    if (!state.interactive || !state.hwnd || state.dragging) {
        return;
    }

    HDC hdc = GetDC(state.hwnd);
    int roiIndex = 0;
    PreviewState::HoveredControl control = PreviewState::HoveredControl::None;
    ResizeHandle resizeHandle = ResizeHandle::None;
    const bool hit = hitTestInteractive(state, point, hdc, &roiIndex, &control, &resizeHandle);
    ReleaseDC(state.hwnd, hdc);

    const int nextRoiIndex = hit ? roiIndex : 0;
    const auto nextControl = hit ? control : PreviewState::HoveredControl::None;
    const ResizeHandle nextResizeHandle = hit ? resizeHandle : ResizeHandle::None;
    if (state.hoveredRoiIndex == nextRoiIndex && state.hoveredControl == nextControl
        && state.hoveredResizeHandle == nextResizeHandle) {
        return;
    }
    state.hoveredRoiIndex = nextRoiIndex;
    state.hoveredControl = nextControl;
    state.hoveredResizeHandle = nextResizeHandle;
    InvalidateRect(state.hwnd, nullptr, FALSE);
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
    case WM_MOUSEMOVE: {
        if (!state || !state->interactive) {
            break;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (state->dragging) {
            PreviewState::RoiItem* item = findRoiItem(*state, state->dragRoiIndex);
            if (item) {
                const int dx = x - state->dragStartPoint.x();
                const int dy = y - state->dragStartPoint.y();
                const QRect next =
                    rectFromResize(state->dragStartRect, state->dragHandle, dx, dy, targetClientBounds(*state));
                if (!next.isEmpty()) {
                    item->clientRect = next;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        updateHoverFromPoint(*state, QPoint(x, y));
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (!state || !state->interactive || state->dragging) {
            break;
        }
        if (state->hoveredRoiIndex != 0 || state->hoveredControl != PreviewState::HoveredControl::None
            || state->hoveredResizeHandle != ResizeHandle::None) {
            state->hoveredRoiIndex = 0;
            state->hoveredControl = PreviewState::HoveredControl::None;
            state->hoveredResizeHandle = ResizeHandle::None;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_SETCURSOR: {
        if (!state || !state->interactive) {
            break;
        }
        if (state->dragging) {
            SetCursor(cursorForResizeHandle(state->dragHandle));
            return TRUE;
        }
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        HDC hdc = GetDC(hwnd);
        int roiIndex = 0;
        PreviewState::HoveredControl control = PreviewState::HoveredControl::None;
        ResizeHandle resizeHandle = ResizeHandle::None;
        const bool hit = hitTestInteractive(*state, QPoint(pt.x, pt.y), hdc, &roiIndex, &control, &resizeHandle);
        ReleaseDC(hwnd, hdc);
        if (hit && control == PreviewState::HoveredControl::None && resizeHandle != ResizeHandle::None) {
            SetCursor(cursorForResizeHandle(resizeHandle));
            return TRUE;
        }
        if (hit && control != PreviewState::HoveredControl::None) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        if (hit) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    }
    case WM_LBUTTONUP: {
        if (!state || !state->interactive || !state->dragging) {
            break;
        }
        state->dragging = false;
        ReleaseCapture();
        PreviewState::RoiItem* item = findRoiItem(*state, state->dragRoiIndex);
        if (item && item->clientRect != state->dragStartRect) {
            invokeRoiRegionHandler(state->onRoiRegionChanged,
                                   state->dragRoiIndex,
                                   clientRectToCaptureRegion(*state, item->clientRect));
        }
        state->dragHandle = ResizeHandle::None;
        state->dragRoiIndex = 0;
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        updateHoverFromPoint(*state, QPoint(x, y));
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!state || !state->interactive) {
            break;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        HDC hdc = GetDC(hwnd);
        int roiIndex = 0;
        PreviewState::HoveredControl control = PreviewState::HoveredControl::None;
        ResizeHandle resizeHandle = ResizeHandle::None;
        const bool hit = hitTestInteractive(*state, QPoint(x, y), hdc, &roiIndex, &control, &resizeHandle);
        ReleaseDC(hwnd, hdc);
        if (!hit) {
            break;
        }

        if (control == PreviewState::HoveredControl::Delete) {
            invokeRoiHandler(state->onRoiDelete, roiIndex);
            return 0;
        }
        if (control == PreviewState::HoveredControl::Add) {
            invokeRoiAction(state->onRoiAdd);
            return 0;
        }

        if (state->selectedRoiIndex != roiIndex) {
            state->selectedRoiIndex = roiIndex;
            InvalidateRect(hwnd, nullptr, TRUE);
            invokeRoiHandler(state->onRoiSelected, roiIndex);
            if (resizeHandle == ResizeHandle::None) {
                return 0;
            }
        }

        if (resizeHandle != ResizeHandle::None) {
            PreviewState::RoiItem* item = findRoiItem(*state, roiIndex);
            if (!item) {
                return 0;
            }
            state->dragging = true;
            state->dragRoiIndex = roiIndex;
            state->dragHandle = resizeHandle;
            state->dragStartPoint = QPoint(x, y);
            state->dragStartRect = item->clientRect;
            SetCapture(hwnd);
            return 0;
        }

        invokeRoiHandler(state->onRoiSelected, roiIndex);
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

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (!state.interactive) {
        exStyle |= WS_EX_TRANSPARENT;
    }

    state.hwnd = CreateWindowExW(exStyle,
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
                             int selectedRoiIndex,
                             bool interactive,
                             RoiIndexHandler onRoiSelected,
                             RoiActionHandler onRoiAdd,
                             RoiRegionHandler onRoiRegionChanged,
                             RoiIndexHandler onRoiDelete) {
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
    g_state->interactive = interactive;
    g_state->onVisibilityChanged = std::move(onVisibilityChanged);
    g_state->onRoiSelected = std::move(onRoiSelected);
    g_state->onRoiAdd = std::move(onRoiAdd);
    g_state->onRoiRegionChanged = std::move(onRoiRegionChanged);
    g_state->onRoiDelete = std::move(onRoiDelete);
    if (interactive) {
        g_state->hintText = QObject::tr("탐색 ROI 미리보기\n"
                                        "ROI 드래그로 이동 · 모서리/변으로 크기 조절 · 추가/삭제 · Esc로 닫기")
                                .toStdWString();
    } else {
        g_state->hintText =
            QObject::tr("탐색 ROI 미리보기 (클릭 통과)\nROI 미리보기 끄기 또는 Esc로 닫기").toStdWString();
    }

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
    (void)interactive;
    (void)onRoiSelected;
    (void)onRoiAdd;
    (void)onRoiRegionChanged;
    (void)onRoiDelete;
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
