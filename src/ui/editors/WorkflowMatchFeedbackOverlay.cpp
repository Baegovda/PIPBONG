#include "ui/editors/WorkflowMatchFeedbackOverlay.h"

#include "app/PointerFeedbackSettings.h"
#include "core/capture/ScreenCapture.h"
#include "ui/ClickPointerFeedbackRenderer.h"
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
constexpr wchar_t kOverlayClassName[] = L"SbmWorkflowMatchFeedbackOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 33;
constexpr UINT kHideForCaptureMsg = WM_APP + 42;
constexpr ULONGLONG kDefaultMatchPulseLifetimeMs = 460;
constexpr size_t kMaxActivePulses = 16;
struct Pulse {
    int clientX = 0;
    int clientY = 0;
    RunPointerFeedbackKind kind = RunPointerFeedbackKind::MatchMiss;
    ULONGLONG startMs = 0;
};

struct OverlayState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    std::vector<Pulse> pulses;
};

std::unique_ptr<OverlayState> g_state;

ULONGLONG nowMs() {
    return GetTickCount64();
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

ULONGLONG pulseLifetimeMs(RunPointerFeedbackKind kind) {
    if (kind == RunPointerFeedbackKind::Click) {
        return static_cast<ULONGLONG>(PointerFeedbackSettings::click().displayDurationMs);
    }
    return kDefaultMatchPulseLifetimeMs;
}

void pruneExpiredPulses(ULONGLONG currentMs) {
    if (!g_state) {
        return;
    }
    auto& pulses = g_state->pulses;
    pulses.erase(std::remove_if(pulses.begin(),
                                pulses.end(),
                                [currentMs](const Pulse& pulse) {
                                    return currentMs - pulse.startMs > pulseLifetimeMs(pulse.kind);
                                }),
                 pulses.end());
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
    const uint8_t outR = blend(r, existingR);
    const uint8_t outG = blend(g, existingG);
    const uint8_t outB = blend(b, existingB);
    pixel = (static_cast<uint32_t>(outA) << 24) | (static_cast<uint32_t>(outR) << 16)
            | (static_cast<uint32_t>(outG) << 8) | static_cast<uint32_t>(outB);
}

void fillCircle(uint32_t* bits,
                int width,
                int height,
                int centerX,
                int centerY,
                int radius,
                uint8_t a,
                uint8_t r,
                uint8_t g,
                uint8_t b) {
    if (radius <= 0) {
        return;
    }
    const int radiusSq = radius * radius;
    for (int y = centerY - radius; y <= centerY + radius; ++y) {
        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            const int dx = x - centerX;
            const int dy = y - centerY;
            if (dx * dx + dy * dy <= radiusSq) {
                setPixelArgb(bits, width, height, x, y, a, r, g, b);
            }
        }
    }
}

void strokeRing(uint32_t* bits,
                int width,
                int height,
                int centerX,
                int centerY,
                int radius,
                int thickness,
                uint8_t a,
                uint8_t r,
                uint8_t g,
                uint8_t b) {
    if (radius <= 0 || thickness <= 0) {
        return;
    }
    const int outerSq = radius * radius;
    const int inner = std::max(0, radius - thickness);
    const int innerSq = inner * inner;
    for (int y = centerY - radius; y <= centerY + radius; ++y) {
        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            const int dx = x - centerX;
            const int dy = y - centerY;
            const int distSq = dx * dx + dy * dy;
            if (distSq <= outerSq && distSq >= innerSq) {
                setPixelArgb(bits, width, height, x, y, a, r, g, b);
            }
        }
    }
}

void renderMatchPulse(uint32_t* pixels,
                      int width,
                      int height,
                      const Pulse& pulse,
                      ULONGLONG age,
                      ULONGLONG lifetimeMs) {
    const float t = static_cast<float>(age) / static_cast<float>(lifetimeMs);
    const float fade = 1.0f - t;
    const uint8_t alpha = static_cast<uint8_t>(std::clamp(fade * 220.0f, 0.0f, 220.0f));

    uint8_t r = 255;
    uint8_t g = 110;
    uint8_t b = 120;
    if (pulse.kind == RunPointerFeedbackKind::MatchSuccess) {
        r = 90;
        g = 240;
        b = 150;
    }

    const int centerX = pulse.clientX;
    const int centerY = pulse.clientY;
    fillCircle(pixels, width, height, centerX, centerY, 4, alpha, r, g, b);

    for (int ring = 0; ring < 2; ++ring) {
        const float ringT =
            std::clamp((age - static_cast<ULONGLONG>(ring) * 90ULL) / 320.0f, 0.0f, 1.0f);
        if (ringT <= 0.0f) {
            continue;
        }
        const int radius = 8 + static_cast<int>(ringT * 26.0f);
        const uint8_t ringAlpha = static_cast<uint8_t>(std::clamp((1.0f - ringT) * 180.0f, 0.0f, 180.0f));
        strokeRing(pixels, width, height, centerX, centerY, radius, 3, ringAlpha, r, g, b);
    }
}

void renderClickPulse(uint32_t* pixels,
                      int width,
                      int height,
                      const Pulse& pulse,
                      ULONGLONG age,
                      ULONGLONG lifetimeMs) {
    renderClickPointerFeedbackFrame(pixels,
                                    width,
                                    height,
                                    pulse.clientX,
                                    pulse.clientY,
                                    age,
                                    lifetimeMs,
                                    PointerFeedbackSettings::click());
}

void renderOverlayFrame() {
    if (!g_state || !g_state->hwnd) {
        return;
    }

    const int width = g_state->physicalBounds.width();
    const int height = g_state->physicalBounds.height();
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

    const ULONGLONG currentMs = nowMs();
    for (const Pulse& pulse : g_state->pulses) {
        const ULONGLONG lifetime = pulseLifetimeMs(pulse.kind);
        const ULONGLONG age = currentMs - pulse.startMs;
        if (age > lifetime) {
            continue;
        }

        if (pulse.kind == RunPointerFeedbackKind::Click) {
            renderClickPulse(pixels, width, height, pulse, age, lifetime);
        } else {
            renderMatchPulse(pixels, width, height, pulse, age, lifetime);
        }
    }

    POINT ptDst{g_state->physicalBounds.x(), g_state->physicalBounds.y()};
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

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kHideForCaptureMsg:
        if (g_state) {
            g_state->pulses.clear();
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId) {
            if (!g_state) {
                KillTimer(hwnd, kTimerId);
                DestroyWindow(hwnd);
                return 0;
            }
            pruneExpiredPulses(nowMs());
            if (g_state->pulses.empty()) {
                WorkflowMatchFeedbackOverlay::dismissAll();
                return 0;
            }
            renderOverlayFrame();
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

bool ensureOverlayWindow() {
    if (!ScreenCapture::findTargetWindow()) {
        return false;
    }
    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        return false;
    }

    const QRect physicalBounds(target.x, target.y, target.width, target.height);
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

#endif // _WIN32

} // namespace

void WorkflowMatchFeedbackOverlay::hideBeforeCapture() {
#ifdef _WIN32
    if (g_state && g_state->hwnd) {
        SendMessageW(g_state->hwnd, kHideForCaptureMsg, 0, 0);
    }
#else
    (void)0;
#endif
}

void WorkflowMatchFeedbackOverlay::pulseAtClientPoint(int clientX,
                                                      int clientY,
                                                      RunPointerFeedbackKind kind) {
#ifdef _WIN32
    if (!ensureOverlayWindow()) {
        return;
    }

    Pulse pulse;
    pulse.clientX = clientX;
    pulse.clientY = clientY;
    pulse.kind = kind;
    pulse.startMs = nowMs();
    g_state->pulses.push_back(pulse);
    if (g_state->pulses.size() > kMaxActivePulses) {
        g_state->pulses.erase(g_state->pulses.begin(),
                              g_state->pulses.begin()
                                  + static_cast<std::ptrdiff_t>(g_state->pulses.size() - kMaxActivePulses));
    }
    renderOverlayFrame();
#else
    (void)clientX;
    (void)clientY;
    (void)kind;
#endif
}

void WorkflowMatchFeedbackOverlay::dismissAll() {
#ifdef _WIN32
    destroyOverlayWindow();
#endif
}
