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
constexpr wchar_t kOverlayClassName[] = L"PipbongTargetWindowHighlightOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 16;
constexpr ULONGLONG kFlashDurationMs = 2400;
constexpr ULONGLONG kSelectionWaveDurationMs = 1000;
constexpr int kBorderThickness = 5;

enum class HighlightMode {
    BorderPulse,
    SelectionWave,
};

struct OverlayState {
    HWND hwnd = nullptr;
    HWND targetHwnd = nullptr;
    ULONGLONG startMs = 0;
    HighlightMode mode = HighlightMode::BorderPulse;
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

float easeOutExpo(float t) {
    return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

float easeOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

float smoothstepf(float edge0, float edge1, float x) {
    const float v = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return v * v * (3.0f - 2.0f * v);
}

// Expanding luminous ring with gaussian-soft profile (radar ping look):
// bright near-white core at the wavefront, colored halo trailing behind.
void drawSoftRing(uint32_t* pixels,
                  int width,
                  int height,
                  float radius,
                  float sigma,
                  float maxAlpha,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b) {
    if (maxAlpha <= 1.0f || radius <= 0.0f) {
        return;
    }
    const float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const float reach = sigma * 3.0f;
    const float outerR = radius + reach;
    const float innerR = std::max(0.0f, radius - reach);
    const float outerSq = outerR * outerR;
    const float innerSq = innerR * innerR;
    const float invTwoSigmaSq = 1.0f / (2.0f * sigma * sigma);

    const int top = std::max(0, static_cast<int>(std::floor(cy - outerR)));
    const int bottom = std::min(height - 1, static_cast<int>(std::ceil(cy + outerR)));

    for (int y = top; y <= bottom; ++y) {
        const float dy = static_cast<float>(y) - cy;
        const float outerDxSq = outerSq - dy * dy;
        if (outerDxSq < 0.0f) {
            continue;
        }
        const float outerDx = std::sqrt(outerDxSq);
        const float innerDxSq = innerSq - dy * dy;
        const float innerDx = innerDxSq > 0.0f ? std::sqrt(innerDxSq) : -1.0f;

        // Left and right ring-band spans on this row; hole in the middle skipped.
        const auto drawSpan = [&](int fromX, int toX) {
            fromX = std::max(0, fromX);
            toX = std::min(width - 1, toX);
            for (int x = fromX; x <= toX; ++x) {
                const float dx = static_cast<float>(x) - cx;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float d = dist - radius;
                const float falloff = std::exp(-d * d * invTwoSigmaSq);
                const float a = maxAlpha * falloff;
                if (a < 1.5f) {
                    continue;
                }
                // Blend toward white at the wavefront core for a luminous look.
                const float core = std::clamp(falloff * falloff, 0.0f, 1.0f) * 0.75f;
                const uint8_t cr = static_cast<uint8_t>(r + (255 - r) * core);
                const uint8_t cg = static_cast<uint8_t>(g + (255 - g) * core);
                const uint8_t cb = static_cast<uint8_t>(b + (255 - b) * core);
                setPixelArgb(pixels, width, height, x, y, static_cast<uint8_t>(a), cr, cg, cb);
            }
        };
        if (innerDx >= 0.0f) {
            drawSpan(static_cast<int>(std::floor(cx - outerDx)), static_cast<int>(std::ceil(cx - innerDx)));
            drawSpan(static_cast<int>(std::floor(cx + innerDx)), static_cast<int>(std::ceil(cx + outerDx)));
        } else {
            drawSpan(static_cast<int>(std::floor(cx - outerDx)), static_cast<int>(std::ceil(cx + outerDx)));
        }
    }
}

// Four L-shaped corner brackets sliding in from outside (lock-on look).
void drawCornerBrackets(uint32_t* pixels,
                        int width,
                        int height,
                        float slideOffset,
                        uint8_t alpha,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b) {
    if (alpha == 0) {
        return;
    }
    const int armLength = std::clamp(static_cast<int>(std::min(width, height) * 0.075f), 24, 140);
    const int thickness = std::clamp(static_cast<int>(std::min(width, height) * 0.008f), 4, 10);
    const int inset = std::clamp(static_cast<int>(std::min(width, height) * 0.02f), 10, 48);

    const auto fillRect = [&](int x0, int y0, int x1, int y1) {
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(width - 1, x1);
        y1 = std::min(height - 1, y1);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                setPixelArgb(pixels, width, height, x, y, alpha, r, g, b);
            }
        }
    };

    const int slide = static_cast<int>(slideOffset);
    // Each bracket slides diagonally outward by `slide` px until it settles.
    const int lx = inset - slide;
    const int ty = inset - slide;
    const int rx = width - 1 - inset + slide;
    const int by = height - 1 - inset + slide;

    // Top-left
    fillRect(lx, ty, lx + armLength, ty + thickness - 1);
    fillRect(lx, ty, lx + thickness - 1, ty + armLength);
    // Top-right
    fillRect(rx - armLength, ty, rx, ty + thickness - 1);
    fillRect(rx - thickness + 1, ty, rx, ty + armLength);
    // Bottom-left
    fillRect(lx, by - thickness + 1, lx + armLength, by);
    fillRect(lx, by - armLength, lx + thickness - 1, by);
    // Bottom-right
    fillRect(rx - armLength, by - thickness + 1, rx, by);
    fillRect(rx - thickness + 1, by - armLength, rx, by);
}

// Crisp 2px frame line + soft inward glow gradient along all edges.
void drawEdgeGlow(uint32_t* pixels,
                  int width,
                  int height,
                  float coreAlpha,
                  float glowAlpha,
                  int glowWidth,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b) {
    if (coreAlpha < 1.0f && glowAlpha < 1.0f) {
        return;
    }
    glowWidth = std::max(4, glowWidth);
    const auto alphaForDepth = [&](int d) -> uint8_t {
        if (d < 2) {
            return static_cast<uint8_t>(std::clamp(coreAlpha, 0.0f, 255.0f));
        }
        const float f = 1.0f - static_cast<float>(d - 2) / static_cast<float>(glowWidth);
        if (f <= 0.0f) {
            return 0;
        }
        return static_cast<uint8_t>(std::clamp(glowAlpha * f * f, 0.0f, 255.0f));
    };

    const int band = glowWidth + 2;
    for (int y = 0; y < height; ++y) {
        const int edgeDistY = std::min(y, height - 1 - y);
        if (edgeDistY < band) {
            for (int x = 0; x < width; ++x) {
                const int d = std::min(edgeDistY, std::min(x, width - 1 - x));
                const uint8_t a = alphaForDepth(d);
                if (a > 0) {
                    setPixelArgb(pixels, width, height, x, y, a, r, g, b);
                }
            }
        } else {
            for (int x = 0; x < band && x < width; ++x) {
                const uint8_t a = alphaForDepth(x);
                if (a > 0) {
                    setPixelArgb(pixels, width, height, x, y, a, r, g, b);
                    setPixelArgb(pixels, width, height, width - 1 - x, y, a, r, g, b);
                }
            }
        }
    }
}

bool targetPhysicalBounds(QRect& boundsOut) {
    if (!g_state || !g_state->targetHwnd || !IsWindow(g_state->targetHwnd)) {
        return false;
    }
    const ScreenCapture::WindowBounds bounds = ScreenCapture::physicalRectForWindow(g_state->targetHwnd);
    if (!bounds.valid || bounds.width <= 0 || bounds.height <= 0) {
        return false;
    }
    boundsOut = QRect(bounds.x, bounds.y, bounds.width, bounds.height);
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
    if (g_state->mode == HighlightMode::SelectionWave) {
        // Modern lock-on confirmation: luminous radar ping expanding from center,
        // edge glow frame breathing in behind it, corner brackets settling with
        // a springy overshoot, then a clean fade.
        const float t = clampedPulse;
        const float minDim = static_cast<float>(std::min(width, height));
        const float maxRadius =
            std::sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height) * 0.5f;

        // Global fade-out over the last 18%.
        const float fadeOut = 1.0f - smoothstepf(0.82f, 1.0f, t);

        // Primary ping ring: fast start, gliding finish.
        const float ringT = std::clamp(t / 0.62f, 0.0f, 1.0f);
        const float ringRadius = maxRadius * easeOutExpo(ringT);
        const float ringSigma = std::max(10.0f, minDim * 0.018f) * (0.7f + 0.9f * ringT);
        const float ringFade = (1.0f - smoothstepf(0.72f, 1.0f, ringT)) * fadeOut;
        drawSoftRing(pixels, width, height, ringRadius, ringSigma, 205.0f * ringFade, 56, 189, 248);

        // Trailing echo ring: thinner, dimmer, slightly behind.
        const float echoT = std::clamp((t - 0.10f) / 0.62f, 0.0f, 1.0f);
        if (echoT > 0.0f) {
            const float echoRadius = maxRadius * easeOutExpo(echoT) * 0.82f;
            const float echoFade = (1.0f - smoothstepf(0.65f, 1.0f, echoT)) * fadeOut;
            drawSoftRing(pixels, width, height, echoRadius,
                         std::max(6.0f, minDim * 0.009f), 90.0f * echoFade, 125, 211, 252);
        }

        // Center flash at the very beginning (quick bloom that dissolves).
        const float flashFade = 1.0f - smoothstepf(0.0f, 0.22f, t);
        if (flashFade > 0.0f) {
            const float flashRadius = minDim * 0.05f * (1.0f + 2.4f * smoothstepf(0.0f, 0.22f, t));
            drawSoftRing(pixels, width, height, flashRadius * 0.5f, flashRadius * 0.6f,
                         170.0f * flashFade * fadeOut, 186, 230, 253);
        }

        // Edge glow frame: rises as the ping reaches the borders.
        const float frameIn = smoothstepf(0.30f, 0.58f, t);
        if (frameIn > 0.0f) {
            const int glowWidth = std::clamp(static_cast<int>(minDim * 0.018f), 8, 26);
            drawEdgeGlow(pixels, width, height,
                         185.0f * frameIn * fadeOut,
                         70.0f * frameIn * fadeOut,
                         glowWidth, 56, 189, 248);
        }

        // Corner brackets: slide in from outside with a spring overshoot.
        const float bracketT = std::clamp((t - 0.42f) / 0.34f, 0.0f, 1.0f);
        if (bracketT > 0.0f) {
            const float slideMax = minDim * 0.05f;
            const float slide = slideMax * (1.0f - easeOutBack(bracketT));
            const uint8_t bracketAlpha = static_cast<uint8_t>(
                std::clamp(235.0f * smoothstepf(0.0f, 0.4f, bracketT) * fadeOut, 0.0f, 255.0f));
            drawCornerBrackets(pixels, width, height, slide, bracketAlpha, 224, 242, 254);
        }
    } else {
        const uint8_t alpha = static_cast<uint8_t>(std::clamp(80.0f + clampedPulse * 175.0f, 0.0f, 255.0f));
        drawBorderFrame(pixels, width, height, kBorderThickness, alpha, 96, 165, 250);
    }

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

float selectionWaveProgress(ULONGLONG elapsedMs) {
    // Linear timeline; per-element easing happens inside renderOverlayFrame.
    return std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(kSelectionWaveDurationMs),
                      0.0f,
                      1.0f);
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == kTimerId && g_state) {
            const ULONGLONG elapsed = nowMs() - g_state->startMs;
            const ULONGLONG duration =
                g_state->mode == HighlightMode::SelectionWave ? kSelectionWaveDurationMs : kFlashDurationMs;
            if (elapsed >= duration) {
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
            const float progress = g_state->mode == HighlightMode::SelectionWave
                                       ? selectionWaveProgress(elapsed)
                                       : pulseStrength(elapsed);
            renderOverlayFrame(bounds, progress);
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

bool createOverlayWindow(HWND targetHwnd, const QRect& physicalBounds, HighlightMode mode) {
    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    g_state = std::make_unique<OverlayState>();
    g_state->targetHwnd = targetHwnd;
    g_state->startMs = nowMs();
    g_state->mode = mode;

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
    renderOverlayFrame(physicalBounds, mode == HighlightMode::SelectionWave ? 0.0f : 1.0f);
    return true;
}

bool showHighlight(HWND targetHwnd, HighlightMode mode, QWidget* hostWidget, bool showMissingTargetMessage) {
    destroyOverlayWindow();

    if (!targetHwnd || !IsWindow(targetHwnd)) {
        if (showMissingTargetMessage) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("창 표시"),
                                 QObject::tr("대상 창을 찾을 수 없습니다.\n"
                                             "먼저 '창 지정'으로 대상 창을 선택하세요."));
        }
        return false;
    }

    const ScreenCapture::WindowBounds bounds = ScreenCapture::physicalRectForWindow(targetHwnd);
    if (!bounds.valid || bounds.width <= 0 || bounds.height <= 0) {
        if (showMissingTargetMessage) {
            QMessageBox::warning(messageBoxParent(hostWidget),
                                 QObject::tr("창 표시"),
                                 QObject::tr("대상 창 영역을 확인할 수 없습니다."));
        }
        return false;
    }

    const QRect physicalBounds(bounds.x, bounds.y, bounds.width, bounds.height);
    return createOverlayWindow(targetHwnd, physicalBounds, mode);
}

#endif // _WIN32

} // namespace

bool TargetWindowHighlightOverlay::flash(QWidget* hostWidget) {
#ifdef _WIN32
    const HWND targetHwnd = ScreenCapture::findTargetWindow();
    return showHighlight(targetHwnd, HighlightMode::BorderPulse, hostWidget, true);
#else
    (void)hostWidget;
    return false;
#endif
}

bool TargetWindowHighlightOverlay::flashSelectionWave(QWidget* hostWidget) {
#ifdef _WIN32
    const HWND targetHwnd = ScreenCapture::findTargetWindow();
    return showHighlight(targetHwnd, HighlightMode::SelectionWave, hostWidget, true);
#else
    (void)hostWidget;
    return false;
#endif
}

bool TargetWindowHighlightOverlay::flashForHwnd(void* hwnd, QWidget* hostWidget) {
#ifdef _WIN32
    return showHighlight(static_cast<HWND>(hwnd), HighlightMode::BorderPulse, hostWidget, false);
#else
    (void)hwnd;
    (void)hostWidget;
    return false;
#endif
}

bool TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(void* hwnd, QWidget* hostWidget) {
#ifdef _WIN32
    return showHighlight(static_cast<HWND>(hwnd), HighlightMode::SelectionWave, hostWidget, false);
#else
    (void)hwnd;
    (void)hostWidget;
    return false;
#endif
}

void TargetWindowHighlightOverlay::dismissAll() {
#ifdef _WIN32
    destroyOverlayWindow();
#endif
}
