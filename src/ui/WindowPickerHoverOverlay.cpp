#include "ui/WindowPickerHoverOverlay.h"

#include "core/capture/ScreenCapture.h"
#include "ui/TargetWindowBindingRole.h"

#include <QCoreApplication>
#include <QRect>
#include <QTimer>

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
constexpr wchar_t kOverlayClassName[] = L"PipbongWindowPickerHoverOverlay";
constexpr int kRefreshIntervalMs = 33;
constexpr ULONGLONG kPulsePeriodMs = 900;
constexpr int kBorderThickness = 5;

struct OverlayState {
    HWND hwnd = nullptr;
    HWND hoveredHwnd = nullptr;
    ULONGLONG pulseStartMs = 0;
    TargetWindowBindingRole role = TargetWindowBindingRole::Main;
};

std::unique_ptr<OverlayState> g_state;

void refreshOverlayFrame();

QTimer* hoverRefreshTimer() {
    static QTimer* timer = []() -> QTimer* {
        auto* refreshTimer = new QTimer(QCoreApplication::instance());
        refreshTimer->setInterval(kRefreshIntervalMs);
        refreshTimer->setTimerType(Qt::PreciseTimer);
        QObject::connect(refreshTimer, &QTimer::timeout, refreshTimer, []() { refreshOverlayFrame(); });
        return refreshTimer;
    }();
    return timer;
}

void startHoverRefreshTimer() {
    if (QTimer* timer = hoverRefreshTimer()) {
        if (!timer->isActive()) {
            timer->start();
        }
    }
}

void stopHoverRefreshTimer() {
    if (QTimer* timer = hoverRefreshTimer()) {
        timer->stop();
    }
}

ULONGLONG nowMs() {
    return GetTickCount64();
}

void destroyOverlayWindow() {
    stopHoverRefreshTimer();
    if (!g_state || !g_state->hwnd) {
        g_state.reset();
        return;
    }

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

bool hoveredPhysicalBounds(QRect& boundsOut) {
    if (!g_state || !g_state->hoveredHwnd || !IsWindow(g_state->hoveredHwnd)) {
        return false;
    }
    const ScreenCapture::WindowBounds bounds = ScreenCapture::physicalRectForWindow(g_state->hoveredHwnd);
    if (!bounds.valid || bounds.width <= 0 || bounds.height <= 0) {
        return false;
    }
    boundsOut = QRect(bounds.x, bounds.y, bounds.width, bounds.height);
    return true;
}

void renderOverlayFrame(const QRect& physicalBounds, float pulse, TargetWindowBindingRole role) {
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

    const float clampedPulse = std::clamp(pulse, 0.0f, 1.0f);
    const uint8_t alpha = static_cast<uint8_t>(std::clamp(70.0f + clampedPulse * 185.0f, 0.0f, 255.0f));
    const TargetWindowBindingAccentRgb accent = targetWindowBindingAccentRgb(role);
    drawBorderFrame(pixels, width, height, kBorderThickness, alpha, accent.r, accent.g, accent.b);

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
    const float phase = static_cast<float>(elapsedMs % kPulsePeriodMs) / static_cast<float>(kPulsePeriodMs);
    return 0.5f * (1.0f + std::sin(phase * 2.0f * kPi));
}

void refreshOverlayFrame() {
    if (!g_state || !g_state->hwnd) {
        return;
    }

    QRect bounds;
    if (!hoveredPhysicalBounds(bounds)) {
        WindowPickerHoverOverlay::dismissAll();
        return;
    }

    SetWindowPos(g_state->hwnd,
                 HWND_TOPMOST,
                 bounds.x(),
                 bounds.y(),
                 bounds.width(),
                 bounds.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    const ULONGLONG elapsed = nowMs() - g_state->pulseStartMs;
    renderOverlayFrame(bounds, pulseStrength(elapsed), g_state->role);
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
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

bool ensureOverlayWindow() {
    if (g_state && g_state->hwnd) {
        return true;
    }
    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    g_state = std::make_unique<OverlayState>();
    g_state->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT
                                          | WS_EX_NOACTIVATE,
                                      kOverlayClassName,
                                      L"",
                                      WS_POPUP,
                                      0,
                                      0,
                                      1,
                                      1,
                                      nullptr,
                                      nullptr,
                                      GetModuleHandleW(nullptr),
                                      nullptr);
    if (!g_state->hwnd) {
        g_state.reset();
        return false;
    }

    ShowWindow(g_state->hwnd, SW_SHOWNOACTIVATE);
    return true;
}

#endif // _WIN32

} // namespace

void WindowPickerHoverOverlay::updateHover(HWND hwnd, TargetWindowBindingRole role) {
#ifdef _WIN32
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        dismissAll();
        return;
    }

    const ScreenCapture::WindowBounds bounds = ScreenCapture::physicalRectForWindow(hwnd);
    if (!bounds.valid || bounds.width <= 0 || bounds.height <= 0) {
        dismissAll();
        return;
    }

    const bool targetChanged = !g_state || g_state->hoveredHwnd != hwnd || g_state->role != role;
    if (!ensureOverlayWindow()) {
        return;
    }

    if (targetChanged) {
        g_state->hoveredHwnd = hwnd;
        g_state->role = role;
        g_state->pulseStartMs = nowMs();
    }

    refreshOverlayFrame();
    startHoverRefreshTimer();
#else
    (void)hwnd;
    (void)role;
#endif
}

void WindowPickerHoverOverlay::dismissAll() {
#ifdef _WIN32
    destroyOverlayWindow();
#endif
}
