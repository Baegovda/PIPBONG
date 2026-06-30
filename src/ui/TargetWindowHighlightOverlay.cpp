#include "ui/TargetWindowHighlightOverlay.h"

#include "core/capture/ScreenCapture.h"

#include <QMessageBox>
#include <QObject>
#include <QWidget>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace {

#ifdef _WIN32
constexpr wchar_t kOverlayClassName[] = L"SbmTargetWindowHighlightOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 33;
constexpr ULONGLONG kFlashDurationMs = 2400;
constexpr int kBorderThickness = 5;

struct OverlayState {
    HWND hwnd = nullptr;
    ULONGLONG startMs = 0;
};

std::unique_ptr<OverlayState> g_state;

ULONGLONG nowMs() {
    return GetTickCount64();
}

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
    pixel = (static_cast<uint32_t>(outA) << 24) | (static_cast<uint32_t>(blend(r, existingR)) << 16)
            | (static_cast<uint32_t>(blend(g, existingG)) << 8) | static_cast<uint32_t>(blend(b, existingB));
}

void drawBorderFrame(uint32_t* pixels,
                     int width,
                     int height,
                     int thickness,
                     uint8_t alpha,
                     uint8_t r,
                     uint8_t g,
                     uint8_t b) {
    thickness = std::max(1, thickness);
    for (int t = 0; t < thickness; ++t) {
        for (int x = 0; x < width; ++x) {
            setPixelArgb(pixels, width, height, x, t, alpha, r, g, b);
            setPixelArgb(pixels, width, height, x, height - 1 - t, alpha, r, g, b);
        }
        for (int y = 0; y < height; ++y) {
            setPixelArgb(pixels, width, height, t, y, alpha, r, g, b);
            setPixelArgb(pixels, width, height, width - 1 - t, y, alpha, r, g, b);
        }
    }
}

bool targetPhysicalBounds(QRect& boundsOut) {
    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        return false;
    }
    boundsOut = QRect(target.x, target.y, target.width, target.height);
    return true;
}

void renderOverlayFrame(const QRect& physicalBounds, float pulse) {
    if (!g_state || !g_state->hwnd) {
        return;
    }

    const int width = physicalBounds.width();
    const int height = physicalBounds.height();
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
    HBITMAP bitmap =
        CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(hdcMem, bitmap);
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    auto* pixels = static_cast<uint32_t*>(bits);
    std::fill(pixels, pixels + pixelCount, 0u);

    const float clampedPulse = std::clamp(pulse, 0.0f, 1.0f);
    const uint8_t alpha = static_cast<uint8_t>(std::clamp(80.0f + clampedPulse * 175.0f, 0.0f, 255.0f));
    drawBorderFrame(pixels, width, height, kBorderThickness, alpha, 96, 165, 250);

    POINT ptDst{physicalBounds.x(), physicalBounds.y()};
    SIZE size{width, height};
    POINT ptSrc{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_state->hwnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

float pulseStrength(ULONGLONG elapsedMs) {
    constexpr float kPi = 3.14159265f;
    constexpr int kPulseCount = 4;
    const float t = static_cast<float>(elapsedMs) / static_cast<float>(kFlashDurationMs);
    if (t >= 1.0f) {
        return 0.0f;
    }
    const float wave = 0.5f * (1.0f + std::sin(t * static_cast<float>(kPulseCount) * 2.0f * kPi));
    const float fadeOut = 1.0f - t;
    return wave * fadeOut;
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == kTimerId && g_state) {
            const ULONGLONG elapsed = nowMs() - g_state->startMs;
            if (elapsed >= kFlashDurationMs) {
                TargetWindowHighlightOverlay::dismissAll();
                return 0;
            }

            QRect bounds;
            if (!targetPhysicalBounds(bounds)) {
                TargetWindowHighlightOverlay::dismissAll();
                return 0;
            }

            SetWindowPos(g_state->hwnd,
                         HWND_TOPMOST,
                         bounds.x(),
                         bounds.y(),
                         bounds.width(),
                         bounds.height(),
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
            renderOverlayFrame(bounds, pulseStrength(elapsed));
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

bool createOverlayWindow(const QRect& physicalBounds) {
    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    g_state = std::make_unique<OverlayState>();
    g_state->startMs = nowMs();

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
    renderOverlayFrame(physicalBounds, 1.0f);
    return true;
}

#endif // _WIN32

} // namespace

bool TargetWindowHighlightOverlay::flash(QWidget* hostWidget) {
#ifdef _WIN32
    dismissAll();

    if (!ScreenCapture::findTargetWindow()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("창 표시"),
                             QObject::tr("대상 창을 찾을 수 없습니다.\n"
                                         "먼저 '창 지정'으로 대상 창을 선택하세요."));
        return false;
    }

    QRect physicalBounds;
    if (!targetPhysicalBounds(physicalBounds)) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("창 표시"),
                             QObject::tr("대상 창 영역을 확인할 수 없습니다."));
        return false;
    }

    return createOverlayWindow(physicalBounds);
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
