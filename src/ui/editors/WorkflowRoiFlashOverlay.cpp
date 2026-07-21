#include "ui/editors/WorkflowRoiFlashOverlay.h"

#include "core/capture/ScreenCapture.h"

#include <QRect>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

#ifdef _WIN32
constexpr wchar_t kOverlayClassName[] = L"PipbongWorkflowRoiFlashOverlay";
constexpr wchar_t kSelectionOverlayClassName[] = L"PipbongWorkflowImageFindSelectionRoiOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT_PTR kSelectionTimerId = 2;
constexpr UINT kTimerMs = 200;
constexpr UINT kSelectionTimerMs = 280;
constexpr int kBorderThickness = 2;
constexpr int kSelectionBorderThickness = 1;
constexpr int kMinRoiSize = 2;
/// Outward padding so the drawn border sits outside the real capture ROI.
constexpr int kDisplayOutsetPx = 10;
constexpr int kSelectionDisplayOutsetPx = 6;

struct OverlayState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    std::vector<QRect> clientRects;
};

struct SelectionOverlayState : OverlayState {
    SearchArea searchArea = SearchArea::TargetWindow;
    CaptureRegion customRegion{};
    PercentRegion percentRegion{};
    std::vector<CaptureRegion> customRegions;
};

std::unique_ptr<OverlayState> g_state;
std::unique_ptr<SelectionOverlayState> g_selectionState;

void destroyOverlayWindow() {
    if (!g_state || !g_state->hwnd) {
        g_state.reset();
        return;
    }

    KillTimer(g_state->hwnd, kTimerId);
    DestroyWindow(g_state->hwnd);
    g_state->hwnd = nullptr;
    g_state.reset();
}

QRect roiClientRectOnTargetWindow(const QRect& roiPhysical, const QRect& targetPhysical) {
    QRect roiClient = roiPhysical.translated(-targetPhysical.x(), -targetPhysical.y());
    const QRect targetClient(0, 0, targetPhysical.width(), targetPhysical.height());
    return roiClient.intersected(targetClient);
}

QRect outwardDisplayRect(const QRect& actualRoi, const QRect& targetClient, int displayOutsetPx) {
    if (actualRoi.width() < kMinRoiSize || actualRoi.height() < kMinRoiSize) {
        return {};
    }
    QRect display = actualRoi.adjusted(-displayOutsetPx, -displayOutsetPx, displayOutsetPx, displayOutsetPx);
    display = display.intersected(targetClient);
    if (display.width() < kMinRoiSize || display.height() < kMinRoiSize) {
        return {};
    }
    return display;
}

bool isValidCustomRegion(const CaptureRegion& region) {
    return region.width >= kMinRoiSize && region.height >= kMinRoiSize;
}

std::vector<QRect> resolveDisplayClientRects(SearchArea searchArea,
                                             const CaptureRegion& customRegion,
                                             const PercentRegion& percentRegion,
                                             const std::vector<CaptureRegion>& customRegions,
                                             const QRect& targetPhysical,
                                             int displayOutsetPx) {
    const QRect targetClient(0, 0, targetPhysical.width(), targetPhysical.height());
    std::vector<QRect> displayRects;

    auto appendDisplayRect = [&](const CaptureRegion& region) {
        QRect roiPhysical;
        if (!ScreenCapture::searchAreaPhysicalRect(
                SearchArea::CustomRegion, region, percentRegion, roiPhysical)) {
            return;
        }
        const QRect actualRoi = roiClientRectOnTargetWindow(roiPhysical, targetPhysical);
        if (const QRect display = outwardDisplayRect(actualRoi, targetClient, displayOutsetPx);
            !display.isEmpty()) {
            displayRects.push_back(display);
        }
    };

    if (searchArea == SearchArea::CustomRegion) {
        if (!customRegions.empty()) {
            for (const CaptureRegion& region : customRegions) {
                if (isValidCustomRegion(region)) {
                    appendDisplayRect(region);
                }
            }
        } else if (isValidCustomRegion(customRegion)) {
            appendDisplayRect(customRegion);
        }
        return displayRects;
    }

    QRect roiPhysical;
    if (!ScreenCapture::searchAreaPhysicalRect(searchArea, customRegion, percentRegion, roiPhysical)) {
        return displayRects;
    }
    const QRect actualRoi = roiClientRectOnTargetWindow(roiPhysical, targetPhysical);
    if (const QRect display = outwardDisplayRect(actualRoi, targetClient, displayOutsetPx); !display.isEmpty()) {
        displayRects.push_back(display);
    }
    return displayRects;
}

void setPixelArgb(uint32_t* bits, int width, int height, int x, int y, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    uint32_t& pixel = bits[y * width + x];
    const uint8_t invA = static_cast<uint8_t>(255 - a);
    const uint8_t existingA = static_cast<uint8_t>((pixel >> 24) & 0xff);
    const uint8_t outA = static_cast<uint8_t>(a + (existingA * invA) / 255);
    const auto blend = [a, invA](uint8_t channel, uint8_t existing) -> uint8_t {
        return static_cast<uint8_t>((channel * a + existing * invA) / 255);
    };
    const uint8_t existingR = static_cast<uint8_t>((pixel >> 16) & 0xff);
    const uint8_t existingG = static_cast<uint8_t>((pixel >> 8) & 0xff);
    const uint8_t existingB = static_cast<uint8_t>(pixel & 0xff);
    pixel = (static_cast<uint32_t>(outA) << 24)
            | (static_cast<uint32_t>(blend(r, existingR)) << 16)
            | (static_cast<uint32_t>(blend(g, existingG)) << 8) | static_cast<uint32_t>(blend(b, existingB));
}

void drawRectBorder(uint32_t* pixels,
                    int width,
                    int height,
                    const QRect& rect,
                    int thickness,
                    uint8_t alpha,
                    uint8_t r,
                    uint8_t g,
                    uint8_t b) {
    if (rect.width() < kMinRoiSize || rect.height() < kMinRoiSize) {
        return;
    }

    const int left = std::clamp(rect.left(), 0, width - 1);
    const int top = std::clamp(rect.top(), 0, height - 1);
    const int right = std::clamp(rect.right(), 0, width - 1);
    const int bottom = std::clamp(rect.bottom(), 0, height - 1);
    thickness = std::max(1, thickness);

    for (int t = 0; t < thickness; ++t) {
        for (int x = left; x <= right; ++x) {
            setPixelArgb(pixels, width, height, x, top + t, alpha, r, g, b);
            setPixelArgb(pixels, width, height, x, bottom - t, alpha, r, g, b);
        }
        for (int y = top; y <= bottom; ++y) {
            setPixelArgb(pixels, width, height, left + t, y, alpha, r, g, b);
            setPixelArgb(pixels, width, height, right - t, y, alpha, r, g, b);
        }
    }
}

void renderOverlayFrame(OverlayState& state,
                        int borderThickness,
                        uint8_t alpha,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b) {
    if (!state.hwnd || state.clientRects.empty()) {
        return;
    }

    const int width = state.physicalBounds.width();
    const int height = state.physicalBounds.height();
    if (width <= 0 || height <= 0) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP bitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(hdcMem, bitmap);
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    auto* pixels = static_cast<uint32_t*>(bits);
    std::fill(pixels, pixels + pixelCount, 0u);

    for (const QRect& rect : state.clientRects) {
        drawRectBorder(pixels, width, height, rect, borderThickness, alpha, r, g, b);
    }

    POINT ptDst{state.physicalBounds.x(), state.physicalBounds.y()};
    SIZE size{width, height};
    POINT ptSrc{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(state.hwnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void renderRunOverlayFrame() {
    if (!g_state) {
        return;
    }
    constexpr uint8_t kAlpha = 185;
    constexpr uint8_t kR = 125;
    constexpr uint8_t kG = 211;
    constexpr uint8_t kB = 252;
    renderOverlayFrame(*g_state, kBorderThickness, kAlpha, kR, kG, kB);
}

void renderSelectionOverlayFrame() {
    if (!g_selectionState) {
        return;
    }
    constexpr uint8_t kAlpha = 108;
    constexpr uint8_t kR = 168;
    constexpr uint8_t kG = 188;
    constexpr uint8_t kB = 214;
    renderOverlayFrame(*g_selectionState, kSelectionBorderThickness, kAlpha, kR, kG, kB);
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == kTimerId && g_state) {
            renderRunOverlayFrame();
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
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
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        return RegisterClassExW(&wc);
    }();
    return atom != 0;
}

bool ensureOverlayWindow(const QRect& physicalBounds) {
    if (g_state && g_state->hwnd && g_state->physicalBounds == physicalBounds) {
        return true;
    }

    if (g_state && g_state->hwnd) {
        KillTimer(g_state->hwnd, kTimerId);
        DestroyWindow(g_state->hwnd);
        g_state->hwnd = nullptr;
    }

    if (!g_state) {
        g_state = std::make_unique<OverlayState>();
    }
    g_state->physicalBounds = physicalBounds;

    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    g_state->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT
                                        | WS_EX_NOACTIVATE,
                                    kOverlayClassName,
                                    L"",
                                    WS_POPUP,
                                    physicalBounds.x(),
                                    physicalBounds.y(),
                                    physicalBounds.width(),
                                    physicalBounds.height(),
                                    nullptr,
                                    nullptr,
                                    GetModuleHandleW(nullptr),
                                    nullptr);
    if (!g_state->hwnd) {
        g_state.reset();
        return false;
    }

    ShowWindow(g_state->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_state->hwnd,
                 HWND_TOPMOST,
                 physicalBounds.x(),
                 physicalBounds.y(),
                 physicalBounds.width(),
                 physicalBounds.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetTimer(g_state->hwnd, kTimerId, kTimerMs, nullptr);
    return true;
}

void destroySelectionOverlayWindow() {
    if (!g_selectionState || !g_selectionState->hwnd) {
        g_selectionState.reset();
        return;
    }

    KillTimer(g_selectionState->hwnd, kSelectionTimerId);
    DestroyWindow(g_selectionState->hwnd);
    g_selectionState->hwnd = nullptr;
    g_selectionState.reset();
}

void refreshSelectionOverlayGeometry() {
    if (!g_selectionState || !g_selectionState->hwnd) {
        return;
    }
    if (!ScreenCapture::findTargetWindow()) {
        destroySelectionOverlayWindow();
        return;
    }

    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        destroySelectionOverlayWindow();
        return;
    }

    const QRect targetPhysical(target.x, target.y, target.width, target.height);
    std::vector<QRect> clientRects = resolveDisplayClientRects(g_selectionState->searchArea,
                                                               g_selectionState->customRegion,
                                                               g_selectionState->percentRegion,
                                                               g_selectionState->customRegions,
                                                               targetPhysical,
                                                               kSelectionDisplayOutsetPx);
    if (clientRects.empty()) {
        destroySelectionOverlayWindow();
        return;
    }

    g_selectionState->physicalBounds = targetPhysical;
    g_selectionState->clientRects = std::move(clientRects);

    SetWindowPos(g_selectionState->hwnd,
                 HWND_TOPMOST,
                 targetPhysical.x(),
                 targetPhysical.y(),
                 targetPhysical.width(),
                 targetPhysical.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    renderSelectionOverlayFrame();
}

LRESULT CALLBACK selectionOverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == kSelectionTimerId && g_selectionState) {
            refreshSelectionOverlayGeometry();
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, kSelectionTimerId);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensureSelectionOverlayClassRegistered() {
    static const ATOM atom = []() -> ATOM {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = selectionOverlayWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kSelectionOverlayClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        return RegisterClassExW(&wc);
    }();
    return atom != 0;
}

bool ensureSelectionOverlayWindow(const QRect& physicalBounds) {
    if (g_selectionState && g_selectionState->hwnd && g_selectionState->physicalBounds == physicalBounds) {
        return true;
    }

    if (g_selectionState && g_selectionState->hwnd) {
        KillTimer(g_selectionState->hwnd, kSelectionTimerId);
        DestroyWindow(g_selectionState->hwnd);
        g_selectionState->hwnd = nullptr;
    }

    if (!g_selectionState) {
        g_selectionState = std::make_unique<SelectionOverlayState>();
    }
    g_selectionState->physicalBounds = physicalBounds;

    if (!ensureSelectionOverlayClassRegistered()) {
        return false;
    }

    g_selectionState->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT
                                                 | WS_EX_NOACTIVATE,
                                             kSelectionOverlayClassName,
                                             L"",
                                             WS_POPUP,
                                             physicalBounds.x(),
                                             physicalBounds.y(),
                                             physicalBounds.width(),
                                             physicalBounds.height(),
                                             nullptr,
                                             nullptr,
                                             GetModuleHandleW(nullptr),
                                             nullptr);
    if (!g_selectionState->hwnd) {
        g_selectionState.reset();
        return false;
    }

    ShowWindow(g_selectionState->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_selectionState->hwnd,
                 HWND_TOPMOST,
                 physicalBounds.x(),
                 physicalBounds.y(),
                 physicalBounds.width(),
                 physicalBounds.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetTimer(g_selectionState->hwnd, kSelectionTimerId, kSelectionTimerMs, nullptr);
    return true;
}

#endif // _WIN32

} // namespace

void WorkflowRoiFlashOverlay::showSearchArea(SearchArea searchArea,
                                             const CaptureRegion& customRegion,
                                             const PercentRegion& percentRegion,
                                             const std::vector<CaptureRegion>& customRegions) {
#ifdef _WIN32
    if (!ScreenCapture::findTargetWindow()) {
        return;
    }

    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        return;
    }

    const QRect targetPhysical(target.x, target.y, target.width, target.height);
    std::vector<QRect> clientRects =
        resolveDisplayClientRects(searchArea, customRegion, percentRegion, customRegions, targetPhysical, kDisplayOutsetPx);
    if (clientRects.empty()) {
        return;
    }

    if (!ensureOverlayWindow(targetPhysical)) {
        return;
    }

    g_state->clientRects = std::move(clientRects);
    renderRunOverlayFrame();
#else
    (void)searchArea;
    (void)customRegion;
    (void)percentRegion;
    (void)customRegions;
#endif
}

void WorkflowRoiFlashOverlay::dismissAll() {
#ifdef _WIN32
    destroyOverlayWindow();
#endif
}

void WorkflowImageFindSelectionRoiOverlay::showSearchArea(SearchArea searchArea,
                                                          const CaptureRegion& customRegion,
                                                          const PercentRegion& percentRegion,
                                                          const std::vector<CaptureRegion>& customRegions) {
#ifdef _WIN32
    if (!ScreenCapture::findTargetWindow()) {
        return;
    }

    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        return;
    }

    const QRect targetPhysical(target.x, target.y, target.width, target.height);
    std::vector<QRect> clientRects = resolveDisplayClientRects(searchArea,
                                                               customRegion,
                                                               percentRegion,
                                                               customRegions,
                                                               targetPhysical,
                                                               kSelectionDisplayOutsetPx);
    if (clientRects.empty()) {
        WorkflowImageFindSelectionRoiOverlay::dismissAll();
        return;
    }

    if (!g_selectionState) {
        g_selectionState = std::make_unique<SelectionOverlayState>();
    }
    g_selectionState->searchArea = searchArea;
    g_selectionState->customRegion = customRegion;
    g_selectionState->percentRegion = percentRegion;
    g_selectionState->customRegions = customRegions;

    if (!ensureSelectionOverlayWindow(targetPhysical)) {
        return;
    }

    g_selectionState->clientRects = std::move(clientRects);
    renderSelectionOverlayFrame();
#else
    (void)searchArea;
    (void)customRegion;
    (void)percentRegion;
    (void)customRegions;
#endif
}

void WorkflowImageFindSelectionRoiOverlay::dismissAll() {
#ifdef _WIN32
    destroySelectionOverlayWindow();
#endif
}
