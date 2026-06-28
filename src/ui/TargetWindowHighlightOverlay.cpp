#include "ui/TargetWindowHighlightOverlay.h"

#include "core/capture/ScreenCapture.h"

#include <QObject>
#include <QMessageBox>
#include <QWidget>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <memory>

namespace {

#ifdef _WIN32
constexpr wchar_t kOverlayClassName[] = L"SbmTargetWindowHighlightOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 200;
constexpr int kMaxTicks = 12;
constexpr int kBorderThickness = 5;

struct OverlayState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    int tickCount = 0;
    bool brightPhase = true;
};

std::unique_ptr<OverlayState> g_state;

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

void setPixelArgb(uint32_t* bits, int width, int height, int x, int y, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= width || y >= height || a == 0) {
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
            | (static_cast<uint32_t>(blend(g, existingG)) << 8)
            | static_cast<uint32_t>(blend(b, existingB));
}

void strokeRectBorder(uint32_t* bits,
                      int width,
                      int height,
                      int thickness,
                      uint8_t a,
                      uint8_t r,
                      uint8_t g,
                      uint8_t b) {
    if (width <= 0 || height <= 0 || thickness <= 0) {
        return;
    }
    const int maxT = std::min({thickness, width / 2, height / 2});
    for (int t = 0; t < maxT; ++t) {
        for (int x = 0; x < width; ++x) {
            setPixelArgb(bits, width, height, x, t, a, r, g, b);
            setPixelArgb(bits, width, height, x, height - 1 - t, a, r, g, b);
        }
        for (int y = 0; y < height; ++y) {
            setPixelArgb(bits, width, height, t, y, a, r, g, b);
            setPixelArgb(bits, width, height, width - 1 - t, y, a, r, g, b);
        }
    }
}

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

bool refreshTargetBounds(OverlayState& state) {
    if (!ScreenCapture::findTargetWindow()) {
        return false;
    }
    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (target.width <= 0 || target.height <= 0) {
        return false;
    }
    state.physicalBounds = QRect(target.x, target.y, target.width, target.height);
    return true;
}

void renderOverlayFrame(OverlayState& state) {
    if (!state.hwnd) {
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

    const uint8_t alpha = state.brightPhase ? 240 : 110;
    strokeRectBorder(pixels, width, height, kBorderThickness, alpha, 80, 190, 255);

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

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayState* state = reinterpret_cast<OverlayState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        state = static_cast<OverlayState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        SetTimer(hwnd, kTimerId, kTimerMs, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wParam == kTimerId && state) {
            ++state->tickCount;
            state->brightPhase = !state->brightPhase;
            if (state->tickCount >= kMaxTicks || !refreshTargetBounds(*state)) {
                TargetWindowHighlightOverlay::dismissAll();
                return 0;
            }
            SetWindowPos(state->hwnd,
                         HWND_TOPMOST,
                         state->physicalBounds.x(),
                         state->physicalBounds.y(),
                         state->physicalBounds.width(),
                         state->physicalBounds.height(),
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
            renderOverlayFrame(*state);
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
        wc.hbrBackground = nullptr;
        return RegisterClassExW(&wc);
    }();
    return atom != 0;
}

bool createOverlayWindow(OverlayState& state) {
    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    state.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                 kOverlayClassName,
                                 L"",
                                 WS_POPUP,
                                 state.physicalBounds.x(),
                                 state.physicalBounds.y(),
                                 state.physicalBounds.width(),
                                 state.physicalBounds.height(),
                                 nullptr,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 &state);
    if (!state.hwnd) {
        return false;
    }

    renderOverlayFrame(state);
    ShowWindow(state.hwnd, SW_SHOWNOACTIVATE);
    return true;
}
#endif

} // namespace

bool TargetWindowHighlightOverlay::isActive() {
#ifdef _WIN32
    return g_state && g_state->hwnd;
#else
    return false;
#endif
}

bool TargetWindowHighlightOverlay::flash(QWidget* hostWidget) {
#ifdef _WIN32
    dismissAll();

    if (!ScreenCapture::findTargetWindow()) {
        if (hostWidget) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("현재 창?"),
                                 QObject::tr("대상 창을 찾을 수 없습니다.\n"
                                              "메인 창에서 '창 지정'을 사용하세요."));
        }
        return false;
    }

    g_state = std::make_unique<OverlayState>();
    if (!refreshTargetBounds(*g_state) || !createOverlayWindow(*g_state)) {
        dismissAll();
        if (hostWidget) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("현재 창?"),
                                 QObject::tr("대상 창 테두리를 표시하지 못했습니다."));
        }
        return false;
    }
    return true;
#else
    (void)hostWidget;
    return false;
#endif
}

void TargetWindowHighlightOverlay::dismissAll() {
#ifdef _WIN32
    destroyOverlayWindow();
#endif
}
