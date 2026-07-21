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
constexpr wchar_t kPreviewClassName[] = L"PipbongRoiPreviewOverlay";
constexpr int kEscHotkeyId = 0x5052;
constexpr int kHandleDrawSize = 7;
constexpr int kHandleHitSize = 18;
constexpr int kHandleHitOutside = 6;
constexpr int kMinRoiSize = 2;
constexpr int kToolbarHeight = 32;
constexpr int kToolbarCornerRadius = 8;
constexpr int kToolbarAttachGap = 6;
constexpr int kToolbarConfirmWidth = 58;
constexpr int kToolbarAddWidth = 52;
constexpr int kToolbarDeleteWidth = 58;
constexpr int kToolbarSegmentGap = 4;
constexpr int kToolbarEdgeInset = 6;

namespace Theme {
constexpr COLORREF kDim = RGB(8, 12, 20);
constexpr COLORREF kAccent = RGB(52, 211, 153);
constexpr COLORREF kAccentBorder = RGB(110, 231, 183);
constexpr COLORREF kGhostBorder = RGB(148, 163, 184);
constexpr COLORREF kGhostFill = RGB(36, 44, 58);
constexpr COLORREF kActiveFill = RGB(14, 58, 48);
constexpr COLORREF kReadOnlyFill = RGB(18, 42, 36);
constexpr COLORREF kPanelFill = RGB(18, 21, 28);
constexpr COLORREF kPanelBorder = RGB(63, 74, 92);
constexpr COLORREF kChipFill = RGB(15, 23, 42);
constexpr COLORREF kChipBorder = RGB(96, 165, 250);
constexpr COLORREF kTextPrimary = RGB(248, 250, 252);
constexpr COLORREF kTextMuted = RGB(186, 198, 214);
constexpr COLORREF kShadow = RGB(6, 8, 12);

constexpr COLORREF kConfirmFill = RGB(16, 82, 48);
constexpr COLORREF kConfirmFillHover = RGB(22, 110, 62);
constexpr COLORREF kConfirmBorder = RGB(52, 211, 153);
constexpr COLORREF kAddFill = RGB(22, 62, 142);
constexpr COLORREF kAddFillHover = RGB(29, 78, 176);
constexpr COLORREF kAddBorder = RGB(96, 165, 250);
constexpr COLORREF kDeleteFill = RGB(120, 28, 28);
constexpr COLORREF kDeleteFillHover = RGB(153, 36, 36);
constexpr COLORREF kDeleteBorder = RGB(248, 113, 113);
} // namespace Theme

void drawRoundRect(HDC hdc, const QRect& rect, int radius, COLORREF fill, COLORREF border, int borderWidth = 1) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, borderWidth, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left(), rect.top(), rect.right() + 1, rect.bottom() + 1, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void drawRoundRectShadow(HDC hdc, const QRect& rect, int radius, int shadowOffset = 2) {
    const QRect shadow = rect.translated(0, shadowOffset);
    drawRoundRect(hdc, shadow, radius, Theme::kShadow, Theme::kShadow);
}

enum class DragMode {
    None,
    Move,
    ResizeNW,
    ResizeN,
    ResizeNE,
    ResizeE,
    ResizeSE,
    ResizeS,
    ResizeSW,
    ResizeW,
    SelectRoi,
};

enum class OverlayButton {
    None,
    Confirm,
    Add,
    Delete,
};

struct PreviewState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    std::vector<QRect> roiClientRects;
    std::wstring hintText;
    bool editable = false;
    int activeIndex = 0;
    bool dragging = false;
    DragMode dragMode = DragMode::None;
    QRect dragStartRect;
    QPoint dragOriginClient;
    RoiPreviewOverlay::VisibilityHandler onVisibilityChanged;
    RoiPreviewOverlay::RoiEditedHandler onRoiEdited;
    RoiPreviewOverlay::RoiSelectedHandler onRoiSelected;
    std::function<void()> onConfirm;
    std::function<void()> onAdd;
    std::function<void()> onDelete;
    std::wstring confirmLabel;
    std::wstring addLabel;
    std::wstring deleteLabel;
    OverlayButton hoverToolbarButton = OverlayButton::None;
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

    if (GetCapture() == g_state->hwnd) {
        ReleaseCapture();
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

QRect targetClientBounds(const PreviewState& state) {
    return QRect(0, 0, state.physicalBounds.width(), state.physicalBounds.height());
}

CaptureRegion clientRectToCaptureRegion(const QRect& clientRect, const QRect& targetPhysical) {
    const QRect physical = clientRect.translated(targetPhysical.x(), targetPhysical.y());
    CaptureRegion region;
    region.x = physical.x();
    region.y = physical.y();
    region.width = physical.width();
    region.height = physical.height();
    return region;
}

QRect clampRectInsideBounds(QRect rect, const QRect& bounds) {
    rect = rect.normalized();
    if (rect.width() < kMinRoiSize) {
        rect.setWidth(kMinRoiSize);
    }
    if (rect.height() < kMinRoiSize) {
        rect.setHeight(kMinRoiSize);
    }
    if (rect.width() > bounds.width()) {
        rect.setWidth(bounds.width());
    }
    if (rect.height() > bounds.height()) {
        rect.setHeight(bounds.height());
    }

    const int maxLeft = bounds.right() - rect.width() + 1;
    const int maxTop = bounds.bottom() - rect.height() + 1;
    rect.moveLeft(std::clamp(rect.left(), bounds.left(), std::max(bounds.left(), maxLeft)));
    rect.moveTop(std::clamp(rect.top(), bounds.top(), std::max(bounds.top(), maxTop)));
    return rect;
}

DragMode hitTestHandles(const QRect& rect, const QPoint& point) {
    const bool inVerticalSpan = point.y() >= rect.top() - kHandleHitOutside
                                && point.y() <= rect.bottom() + kHandleHitOutside;
    const bool inHorizontalSpan = point.x() >= rect.left() - kHandleHitOutside
                                  && point.x() <= rect.right() + kHandleHitOutside;

    const bool left = inVerticalSpan && point.x() >= rect.left() - kHandleHitOutside
                      && point.x() <= rect.left() + kHandleHitSize;
    const bool right = inVerticalSpan && point.x() >= rect.right() - kHandleHitSize
                       && point.x() <= rect.right() + kHandleHitOutside;
    const bool top = inHorizontalSpan && point.y() >= rect.top() - kHandleHitOutside
                     && point.y() <= rect.top() + kHandleHitSize;
    const bool bottom = inHorizontalSpan && point.y() >= rect.bottom() - kHandleHitSize
                        && point.y() <= rect.bottom() + kHandleHitOutside;

    if (top && left) {
        return DragMode::ResizeNW;
    }
    if (top && right) {
        return DragMode::ResizeNE;
    }
    if (bottom && left) {
        return DragMode::ResizeSW;
    }
    if (bottom && right) {
        return DragMode::ResizeSE;
    }
    if (top) {
        return DragMode::ResizeN;
    }
    if (bottom) {
        return DragMode::ResizeS;
    }
    if (left) {
        return DragMode::ResizeW;
    }
    if (right) {
        return DragMode::ResizeE;
    }
    if (rect.contains(point)) {
        return DragMode::Move;
    }
    return DragMode::None;
}

bool isNearActiveRoi(const QRect& rect, const QPoint& point) {
    return rect.adjusted(-kHandleHitOutside, -kHandleHitOutside, kHandleHitOutside, kHandleHitOutside)
        .contains(point);
}

struct HitResult {
    int roiIndex = -1;
    DragMode mode = DragMode::None;
};

HCURSOR cursorForDragMode(DragMode mode) {
    switch (mode) {
    case DragMode::ResizeNW:
    case DragMode::ResizeSE:
        return LoadCursorW(nullptr, IDC_SIZENWSE);
    case DragMode::ResizeNE:
    case DragMode::ResizeSW:
        return LoadCursorW(nullptr, IDC_SIZENESW);
    case DragMode::ResizeN:
    case DragMode::ResizeS:
        return LoadCursorW(nullptr, IDC_SIZENS);
    case DragMode::ResizeW:
    case DragMode::ResizeE:
        return LoadCursorW(nullptr, IDC_SIZEWE);
    case DragMode::Move:
        return LoadCursorW(nullptr, IDC_SIZEALL);
    default:
        return LoadCursorW(nullptr, IDC_ARROW);
    }
}

HitResult hitTestEditable(const PreviewState& state, const QPoint& clientPoint) {
    if (!state.editable || state.roiClientRects.empty()) {
        return {};
    }

    const int active = std::clamp(state.activeIndex, 0, static_cast<int>(state.roiClientRects.size()) - 1);
    if (active >= 0 && active < static_cast<int>(state.roiClientRects.size())) {
        const QRect& activeRect = state.roiClientRects[static_cast<size_t>(active)];
        if (isNearActiveRoi(activeRect, clientPoint)) {
            const DragMode mode = hitTestHandles(activeRect, clientPoint);
            if (mode != DragMode::None) {
                return {active, mode};
            }
        }
    }

    for (int i = static_cast<int>(state.roiClientRects.size()) - 1; i >= 0; --i) {
        if (i == active) {
            continue;
        }
        if (state.roiClientRects[static_cast<size_t>(i)].contains(clientPoint)) {
            return {i, DragMode::SelectRoi};
        }
    }

    return {};
}

QRect activeRoiClientRect(const PreviewState& state) {
    if (state.roiClientRects.empty()) {
        return {};
    }
    const int active = std::clamp(state.activeIndex, 0, static_cast<int>(state.roiClientRects.size()) - 1);
    return state.roiClientRects[static_cast<size_t>(active)];
}

QRect toolbarStripRect(const QRect& activeRoi, const QRect& clientBounds) {
    if (activeRoi.isEmpty()) {
        return {};
    }
    const int totalWidth =
        kToolbarConfirmWidth + kToolbarAddWidth + kToolbarDeleteWidth + kToolbarSegmentGap * 2;
    int left = activeRoi.center().x() - totalWidth / 2;
    int top = activeRoi.top() - kToolbarAttachGap - kToolbarHeight;
    if (top < kToolbarEdgeInset) {
        top = activeRoi.bottom() + kToolbarAttachGap;
    }
    left = std::clamp(left, kToolbarEdgeInset, std::max(kToolbarEdgeInset, clientBounds.width() - totalWidth - kToolbarEdgeInset));
    return QRect(left, top, totalWidth, kToolbarHeight);
}

QRect toolbarButtonRect(const QRect& strip, int index) {
    int x = strip.left();
    int width = kToolbarConfirmWidth;
    if (index == 1) {
        x += kToolbarConfirmWidth + kToolbarSegmentGap;
        width = kToolbarAddWidth;
    } else if (index == 2) {
        x += kToolbarConfirmWidth + kToolbarSegmentGap + kToolbarAddWidth + kToolbarSegmentGap;
        width = kToolbarDeleteWidth;
    }
    return QRect(x, strip.top(), width, strip.height());
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

HFONT createToolbarFont() {
    return createUiFont(-13, FW_BOLD);
}

struct ToolbarSegmentStyle {
    COLORREF fill = Theme::kPanelFill;
    COLORREF border = Theme::kPanelBorder;
    COLORREF text = Theme::kTextPrimary;
};

ToolbarSegmentStyle toolbarSegmentStyle(OverlayButton button, bool enabled, bool hovered) {
    ToolbarSegmentStyle style;
    if (!enabled) {
        style.fill = RGB(34, 40, 52);
        style.border = RGB(71, 85, 105);
        style.text = RGB(100, 116, 139);
        return style;
    }

    switch (button) {
    case OverlayButton::Confirm:
        style.fill = hovered ? Theme::kConfirmFillHover : Theme::kConfirmFill;
        style.border = Theme::kConfirmBorder;
        style.text = RGB(236, 253, 245);
        break;
    case OverlayButton::Add:
        style.fill = hovered ? Theme::kAddFillHover : Theme::kAddFill;
        style.border = Theme::kAddBorder;
        style.text = RGB(239, 246, 255);
        break;
    case OverlayButton::Delete:
        style.fill = hovered ? Theme::kDeleteFillHover : Theme::kDeleteFill;
        style.border = Theme::kDeleteBorder;
        style.text = RGB(254, 242, 242);
        break;
    default:
        break;
    }
    return style;
}

void drawToolbarSegment(HDC hdc,
                        const QRect& rect,
                        OverlayButton button,
                        const std::wstring& label,
                        bool enabled,
                        bool hovered) {
    const ToolbarSegmentStyle style = toolbarSegmentStyle(button, enabled, hovered);
    drawRoundRectShadow(hdc, rect, kToolbarCornerRadius, 2);
    drawRoundRect(hdc, rect, kToolbarCornerRadius, style.fill, style.border, hovered ? 2 : 1);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, style.text);
    RECT textRect{rect.left(), rect.top(), rect.right() + 1, rect.bottom() + 1};
    DrawTextW(hdc, label.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawToolbar(HDC hdc, const PreviewState& state, const QRect& activeRoi) {
    if (!state.editable || activeRoi.isEmpty()) {
        return;
    }

    const QRect strip = toolbarStripRect(activeRoi, targetClientBounds(state));
    if (strip.isEmpty()) {
        return;
    }

    HFONT font = createToolbarFont();
    HGDIOBJ oldFont = SelectObject(hdc, font);

    const bool canDelete = !state.roiClientRects.empty();
    drawToolbarSegment(hdc,
                       toolbarButtonRect(strip, 0),
                       OverlayButton::Confirm,
                       state.confirmLabel,
                       true,
                       state.hoverToolbarButton == OverlayButton::Confirm);
    drawToolbarSegment(hdc,
                       toolbarButtonRect(strip, 1),
                       OverlayButton::Add,
                       state.addLabel,
                       true,
                       state.hoverToolbarButton == OverlayButton::Add);
    drawToolbarSegment(hdc,
                       toolbarButtonRect(strip, 2),
                       OverlayButton::Delete,
                       state.deleteLabel,
                       canDelete,
                       state.hoverToolbarButton == OverlayButton::Delete);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

OverlayButton hitTestToolbar(const PreviewState& state, const QPoint& clientPoint) {
    if (!state.editable) {
        return OverlayButton::None;
    }

    const QRect activeRoi = activeRoiClientRect(state);
    const QRect strip = toolbarStripRect(activeRoi, targetClientBounds(state));
    if (strip.isEmpty() || !strip.contains(clientPoint)) {
        return OverlayButton::None;
    }

    if (toolbarButtonRect(strip, 0).contains(clientPoint)) {
        return OverlayButton::Confirm;
    }
    if (toolbarButtonRect(strip, 1).contains(clientPoint)) {
        return OverlayButton::Add;
    }
    if (toolbarButtonRect(strip, 2).contains(clientPoint) && !state.roiClientRects.empty()) {
        return OverlayButton::Delete;
    }
    return OverlayButton::None;
}

HCURSOR cursorForHover(const PreviewState& state, const QPoint& clientPoint) {
    if (state.dragging) {
        return cursorForDragMode(state.dragMode);
    }
    if (hitTestToolbar(state, clientPoint) != OverlayButton::None) {
        return LoadCursorW(nullptr, IDC_HAND);
    }

    const HitResult hit = hitTestEditable(state, clientPoint);
    if (hit.roiIndex >= 0) {
        if (hit.mode == DragMode::SelectRoi) {
            return LoadCursorW(nullptr, IDC_HAND);
        }
        if (hit.mode != DragMode::None) {
            return cursorForDragMode(hit.mode);
        }
    }
    return LoadCursorW(nullptr, IDC_ARROW);
}

void scheduleToolbarAction(PreviewState& state, OverlayButton button) {
    std::function<void()> callback;
    switch (button) {
    case OverlayButton::Confirm:
        callback = state.onConfirm;
        break;
    case OverlayButton::Add:
        callback = state.onAdd;
        break;
    case OverlayButton::Delete:
        callback = state.onDelete;
        break;
    default:
        break;
    }
    if (callback) {
        QTimer::singleShot(0, qApp, [callback = std::move(callback)]() { callback(); });
    }
}

QRect applyResizeDrag(const QRect& startRect, DragMode mode, const QPoint& currentClient, const QRect& bounds) {
    QRect rect = startRect.normalized();
    switch (mode) {
    case DragMode::ResizeNW:
        rect.setTopLeft(currentClient);
        break;
    case DragMode::ResizeN:
        rect.setTop(currentClient.y());
        break;
    case DragMode::ResizeNE:
        rect.setTop(currentClient.y());
        rect.setRight(currentClient.x());
        break;
    case DragMode::ResizeE:
        rect.setRight(currentClient.x());
        break;
    case DragMode::ResizeSE:
        rect.setBottomRight(currentClient);
        break;
    case DragMode::ResizeS:
        rect.setBottom(currentClient.y());
        break;
    case DragMode::ResizeSW:
        rect.setBottom(currentClient.y());
        rect.setLeft(currentClient.x());
        break;
    case DragMode::ResizeW:
        rect.setLeft(currentClient.x());
        break;
    default:
        break;
    }

    rect = rect.normalized();
    if (rect.width() < kMinRoiSize) {
        rect.setWidth(kMinRoiSize);
    }
    if (rect.height() < kMinRoiSize) {
        rect.setHeight(kMinRoiSize);
    }
    return clampRectInsideBounds(rect, bounds);
}

HFONT createRoiBadgeFont() {
    return createUiFont(-12, FW_BOLD);
}

void drawRoiNumberBadge(HDC hdc, const QRect& roiRect, int index1Based, bool isActive, bool isGhost) {
    const std::wstring label = std::to_wstring(index1Based);

    HFONT font = createRoiBadgeFont();
    HGDIOBJ oldFont = SelectObject(hdc, font);

    SIZE textSize{};
    GetTextExtentPoint32W(hdc, label.c_str(), static_cast<int>(label.size()), &textSize);

    constexpr int kPadX = 9;
    constexpr int kPadY = 5;
    const int badgeWidth = std::max(static_cast<int>(textSize.cx) + kPadX * 2, 26);
    const int badgeHeight = textSize.cy + kPadY * 2;
    const int left = roiRect.left() + 6;
    const int top = roiRect.top() + 6;
    const QRect badgeRect(left, top, badgeWidth, badgeHeight);

    COLORREF fill = Theme::kChipFill;
    COLORREF border = Theme::kChipBorder;
    COLORREF text = Theme::kTextPrimary;
    if (isGhost) {
        fill = RGB(40, 48, 62);
        border = Theme::kGhostBorder;
        text = Theme::kTextMuted;
    } else if (isActive) {
        fill = RGB(16, 96, 58);
        border = Theme::kAccentBorder;
        text = RGB(236, 253, 245);
    }

    drawRoundRectShadow(hdc, badgeRect, 8, 2);
    drawRoundRect(hdc, badgeRect, 8, fill, border, isActive ? 2 : 1);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);
    RECT textRect{badgeRect.left(), badgeRect.top(), badgeRect.right() + 1, badgeRect.bottom() + 1};
    DrawTextW(hdc, label.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void drawSizeChip(HDC hdc, const QRect& roiRect) {
    const std::wstring sizeText =
        std::to_wstring(roiRect.width()) + L" \x00D7 " + std::to_wstring(roiRect.height());

    HFONT font = createUiFont(-11, FW_NORMAL);
    HGDIOBJ oldFont = SelectObject(hdc, font);

    SIZE textSize{};
    GetTextExtentPoint32W(hdc, sizeText.c_str(), static_cast<int>(sizeText.size()), &textSize);

    constexpr int kPadX = 8;
    constexpr int kPadY = 4;
    const int chipWidth = textSize.cx + kPadX * 2;
    const int chipHeight = textSize.cy + kPadY * 2;
    int left = roiRect.right() - chipWidth - 8;
    int top = roiRect.bottom() - chipHeight - 8;
    if (top < roiRect.top() + 8) {
        top = roiRect.top() + 8;
    }
    const QRect chipRect(left, top, chipWidth, chipHeight);

    drawRoundRect(hdc, chipRect, 6, Theme::kChipFill, Theme::kChipBorder);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::kTextMuted);
    RECT textRect{chipRect.left(), chipRect.top(), chipRect.right() + 1, chipRect.bottom() + 1};
    DrawTextW(hdc, sizeText.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void drawResizeHandle(HDC hdc, int centerX, int centerY, int radius) {
    HPEN outerPen = CreatePen(PS_SOLID, 2, Theme::kAccentBorder);
    HBRUSH innerBrush = CreateSolidBrush(RGB(248, 250, 252));
    HGDIOBJ oldPen = SelectObject(hdc, outerPen);
    HGDIOBJ oldBrush = SelectObject(hdc, innerBrush);
    Ellipse(hdc, centerX - radius, centerY - radius, centerX + radius, centerY + radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(innerBrush);
    DeleteObject(outerPen);
}

void drawGhostRoi(HDC hdc, const QRect& roiRect) {
    const RECT inner = {roiRect.left() + 1,
                        roiRect.top() + 1,
                        roiRect.right(),
                        roiRect.bottom()};
    HBRUSH fillBrush = CreateSolidBrush(Theme::kGhostFill);
    FillRect(hdc, &inner, fillBrush);
    DeleteObject(fillBrush);

    HPEN pen = CreatePen(PS_DOT, 2, Theme::kGhostBorder);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, roiRect.left(), roiRect.top(), roiRect.right() + 1, roiRect.bottom() + 1);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void drawActiveRoi(HDC hdc, const QRect& roiRect, bool editable) {
    const RECT inner = {roiRect.left() + 2,
                        roiRect.top() + 2,
                        roiRect.right() - 1,
                        roiRect.bottom() - 1};
    HBRUSH fillBrush = CreateSolidBrush(Theme::kActiveFill);
    FillRect(hdc, &inner, fillBrush);
    DeleteObject(fillBrush);

    HPEN pen = CreatePen(PS_SOLID, 3, Theme::kAccentBorder);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, roiRect.left(), roiRect.top(), roiRect.right() + 1, roiRect.bottom() + 1);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    drawSizeChip(hdc, roiRect);

    if (!editable) {
        return;
    }

    const int hs = kHandleDrawSize;
    const std::vector<QPoint> handleCenters = {
        QPoint(roiRect.left(), roiRect.top()),
        QPoint(roiRect.center().x(), roiRect.top()),
        QPoint(roiRect.right(), roiRect.top()),
        QPoint(roiRect.right(), roiRect.center().y()),
        QPoint(roiRect.right(), roiRect.bottom()),
        QPoint(roiRect.center().x(), roiRect.bottom()),
        QPoint(roiRect.left(), roiRect.bottom()),
        QPoint(roiRect.left(), roiRect.center().y()),
    };
    for (const QPoint& center : handleCenters) {
        drawResizeHandle(hdc, center.x(), center.y(), hs);
    }
}

void drawReadOnlyRoi(HDC hdc, const QRect& roiRect) {
    const RECT inner = {roiRect.left() + 2,
                        roiRect.top() + 2,
                        roiRect.right() - 1,
                        roiRect.bottom() - 1};
    HBRUSH fillBrush = CreateSolidBrush(Theme::kReadOnlyFill);
    FillRect(hdc, &inner, fillBrush);
    DeleteObject(fillBrush);

    HPEN pen = CreatePen(PS_SOLID, 3, Theme::kAccentBorder);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, roiRect.left(), roiRect.top(), roiRect.right() + 1, roiRect.bottom() + 1);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void drawHintBanner(HDC hdc, const RECT& clientRect, const std::wstring& hintText) {
    HFONT font = createUiFont(-12, FW_NORMAL);
    HGDIOBJ oldFont = SelectObject(hdc, font);

    RECT measureRect{clientRect.left + 24,
                     clientRect.bottom - 80,
                     clientRect.right - 24,
                     clientRect.bottom - 16};
    DrawTextW(hdc, hintText.c_str(), -1, &measureRect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);

    constexpr int kPadX = 14;
    constexpr int kPadY = 10;
    QRect bannerRect(measureRect.left - kPadX,
                     measureRect.top - kPadY,
                     measureRect.right + kPadX,
                     measureRect.bottom + kPadY);
    bannerRect = bannerRect.intersected(
        QRect(clientRect.left + 12, clientRect.top + 12, clientRect.right - 12, clientRect.bottom - 12));

    drawRoundRectShadow(hdc, bannerRect, 10, 2);
    drawRoundRect(hdc, bannerRect, 10, Theme::kPanelFill, Theme::kPanelBorder);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::kTextMuted);
    RECT textRect{bannerRect.left() + kPadX - 4,
                  bannerRect.top() + kPadY - 2,
                  bannerRect.right() - kPadX + 4,
                  bannerRect.bottom() - kPadY + 2};
    DrawTextW(hdc, hintText.c_str(), -1, &textRect, DT_LEFT | DT_WORDBREAK);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void paintPreview(HDC hdc, const PreviewState& state) {
    RECT clientRect{};
    GetClientRect(state.hwnd, &clientRect);

    HBRUSH dimBrush = CreateSolidBrush(Theme::kDim);
    FillRect(hdc, &clientRect, dimBrush);
    DeleteObject(dimBrush);

    const int active = state.editable
                           ? std::clamp(state.activeIndex, 0, static_cast<int>(state.roiClientRects.size()) - 1)
                           : -1;

    for (int i = 0; i < static_cast<int>(state.roiClientRects.size()); ++i) {
        const QRect& roiRect = state.roiClientRects[static_cast<size_t>(i)];
        if (roiRect.width() < kMinRoiSize || roiRect.height() < kMinRoiSize) {
            continue;
        }
        if (state.editable && i == active) {
            drawActiveRoi(hdc, roiRect, true);
            drawRoiNumberBadge(hdc, roiRect, i + 1, true, false);
        } else if (state.editable) {
            drawGhostRoi(hdc, roiRect);
            drawRoiNumberBadge(hdc, roiRect, i + 1, false, true);
        } else {
            drawReadOnlyRoi(hdc, roiRect);
            drawRoiNumberBadge(hdc, roiRect, i + 1, true, false);
        }
    }

    if (state.editable && active >= 0) {
        drawToolbar(hdc, state, state.roiClientRects[static_cast<size_t>(active)]);
    }

    if (!state.hintText.empty()) {
        drawHintBanner(hdc, clientRect, state.hintText);
    }
}

void scheduleRoiSelected(PreviewState& state, int index) {
    if (!state.onRoiSelected) {
        return;
    }
    RoiPreviewOverlay::RoiSelectedHandler callback = state.onRoiSelected;
    QTimer::singleShot(0, qApp, [callback, index]() { callback(index); });
}

void scheduleRoiEdited(PreviewState& state, int index, const CaptureRegion& region) {
    if (!state.onRoiEdited) {
        return;
    }
    RoiPreviewOverlay::RoiEditedHandler callback = state.onRoiEdited;
    QTimer::singleShot(0, qApp, [callback, index, region]() { callback(index, region); });
}

void commitDragIfChanged(PreviewState& state) {
    if (!state.editable || state.dragMode == DragMode::None || state.dragMode == DragMode::SelectRoi) {
        return;
    }
    if (state.activeIndex < 0 || state.activeIndex >= static_cast<int>(state.roiClientRects.size())) {
        return;
    }

    const QRect& finalRect = state.roiClientRects[static_cast<size_t>(state.activeIndex)];
    if (finalRect == state.dragStartRect) {
        return;
    }

    scheduleRoiEdited(state, state.activeIndex, clientRectToCaptureRegion(finalRect, state.physicalBounds));
}

LRESULT CALLBACK previewWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PreviewState* state = reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        state = static_cast<PreviewState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        SetLayeredWindowAttributes(hwnd, 0, state->editable ? 100 : 112, LWA_ALPHA);
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
    case WM_SETCURSOR:
        if (state && state->editable && LOWORD(lParam) == HTCLIENT) {
            const DWORD cursorPos = GetMessagePos();
            POINT screenPoint{GET_X_LPARAM(static_cast<LPARAM>(cursorPos)),
                              GET_Y_LPARAM(static_cast<LPARAM>(cursorPos))};
            ScreenToClient(hwnd, &screenPoint);
            SetCursor(cursorForHover(*state, QPoint(screenPoint.x, screenPoint.y)));
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state && state->editable) {
            const QPoint clientPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            const OverlayButton toolbarButton = hitTestToolbar(*state, clientPoint);
            if (toolbarButton != OverlayButton::None) {
                scheduleToolbarAction(*state, toolbarButton);
                return 0;
            }

            const HitResult hit = hitTestEditable(*state, clientPoint);
            if (hit.roiIndex < 0) {
                return 0;
            }

            if (hit.mode == DragMode::SelectRoi) {
                state->activeIndex = hit.roiIndex;
                scheduleRoiSelected(*state, hit.roiIndex);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            state->dragging = true;
            state->dragMode = hit.mode;
            state->activeIndex = hit.roiIndex;
            state->dragStartRect = state->roiClientRects[static_cast<size_t>(hit.roiIndex)];
            state->dragOriginClient = clientPoint;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (state && state->editable) {
            const QPoint clientPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (state->dragging) {
                const QRect bounds = targetClientBounds(*state);
                QRect updated = state->dragStartRect;
                if (state->dragMode == DragMode::Move) {
                    const QPoint delta = clientPoint - state->dragOriginClient;
                    updated = clampRectInsideBounds(state->dragStartRect.translated(delta), bounds);
                } else {
                    updated = applyResizeDrag(state->dragStartRect, state->dragMode, clientPoint, bounds);
                }
                state->roiClientRects[static_cast<size_t>(state->activeIndex)] = updated;
                SetCursor(cursorForDragMode(state->dragMode));
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            const OverlayButton hovered = hitTestToolbar(*state, clientPoint);
            if (hovered != state->hoverToolbarButton) {
                state->hoverToolbarButton = hovered;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            SetCursor(cursorForHover(*state, clientPoint));
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (state && state->editable && state->dragging) {
            state->dragging = false;
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            const QPoint clientPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            const QRect bounds = targetClientBounds(*state);
            QRect updated = state->dragStartRect;
            if (state->dragMode == DragMode::Move) {
                const QPoint delta = clientPoint - state->dragOriginClient;
                updated = clampRectInsideBounds(state->dragStartRect.translated(delta), bounds);
            } else if (state->dragMode != DragMode::SelectRoi) {
                updated = applyResizeDrag(state->dragStartRect, state->dragMode, clientPoint, bounds);
            }
            state->roiClientRects[static_cast<size_t>(state->activeIndex)] = updated;
            commitDragIfChanged(*state);
            state->dragMode = DragMode::None;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
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
    if (!state.editable) {
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

bool RoiPreviewOverlay::isEditable() {
#ifdef _WIN32
    return g_state && g_state->hwnd && g_state->editable;
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
                             const EditableOptions& editable) {
#ifdef _WIN32
    MatchTestOverlay::dismissAll();
    dismissAll();

    if (!ScreenCapture::findTargetWindow()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("타겟을 찾을 수 없습니다.\n"
                                         "메인 창에서 먼저 '타겟 지정'을 사용하세요."));
        return false;
    }

    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("타겟 영역을 확인할 수 없습니다."));
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
        if (roiClient.width() >= kMinRoiSize && roiClient.height() >= kMinRoiSize) {
            roiClientRects.push_back(roiClient);
        }
    };

    if (!customRegions.empty()) {
        for (const CaptureRegion& region : customRegions) {
            if (region.width >= kMinRoiSize && region.height >= kMinRoiSize) {
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
        if (roiClient.width() < kMinRoiSize || roiClient.height() < kMinRoiSize) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("ROI 미리보기"),
                                 QObject::tr("탐색 ROI가 타겟과 겹치지 않습니다."));
            return false;
        }
        roiClientRects.push_back(roiClient);
    }

    if (roiClientRects.empty()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("ROI 미리보기"),
                             QObject::tr("탐색 ROI가 타겟과 겹치지 않습니다."));
        return false;
    }

    g_state = std::make_unique<PreviewState>();
    g_state->physicalBounds = targetPhysical;
    g_state->roiClientRects = std::move(roiClientRects);
    g_state->onVisibilityChanged = std::move(onVisibilityChanged);
    g_state->editable = editable.enabled && !customRegions.empty();
    g_state->activeIndex =
        g_state->editable
            ? std::clamp(editable.activeIndex, 0, static_cast<int>(g_state->roiClientRects.size()) - 1)
            : 0;
    g_state->onRoiEdited = editable.onRoiEdited;
    g_state->onRoiSelected = editable.onRoiSelected;
    g_state->onConfirm = editable.onConfirm;
    g_state->onAdd = editable.onAdd;
    g_state->onDelete = editable.onDelete;
    if (g_state->editable) {
        g_state->confirmLabel = QObject::tr("확인").toStdWString();
        g_state->addLabel = QObject::tr("추가").toStdWString();
        g_state->deleteLabel = QObject::tr("삭제").toStdWString();
        g_state->hintText =
            QObject::tr("탐색 ROI 편집 — 드래그: 이동 · 모서리/변: 크기 · Esc: 닫기").toStdWString();
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
    (void)editable;
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
                               VisibilityHandler onVisibilityChanged,
                               const EditableOptions& editable) {
    if (isVisible()) {
        hide();
        return false;
    }
    return show(searchArea,
                customRegion,
                percentRegion,
                customRegions,
                hostWidget,
                std::move(onVisibilityChanged),
                editable);
}

void RoiPreviewOverlay::setActiveRoiIndex(int index) {
#ifdef _WIN32
    if (!g_state || !g_state->hwnd || !g_state->editable) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(g_state->roiClientRects.size())) {
        return;
    }
    g_state->activeIndex = index;
    InvalidateRect(g_state->hwnd, nullptr, FALSE);
#endif
}

void RoiPreviewOverlay::dismissAll() {
#ifdef _WIN32
    destroyPreviewWindow();
#endif
}
