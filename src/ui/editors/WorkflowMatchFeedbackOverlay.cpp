#include "ui/editors/WorkflowMatchFeedbackOverlay.h"

#include "app/PointerFeedbackSettings.h"
#include "core/capture/ScreenCapture.h"
#include "ui/ClickPointerFeedbackRenderer.h"
#include "ui/editors/FeedbackHideBatch.h"
#include <QRect>
#include <QCoreApplication>
#include <QThread>
#include <QMetaObject>
#include <map>

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
#include <vector>

namespace {

#ifdef _WIN32
constexpr wchar_t kOverlayClassName[] = L"PipbongWorkflowMatchFeedbackOverlay";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 33;
constexpr UINT kHideForCaptureMsg = WM_APP + 42;
constexpr ULONGLONG kDefaultMatchPulseLifetimeMs = 460;
constexpr ULONGLONG kTriggerScanPulseLifetimeMs = 1400;
constexpr size_t kMaxActivePulses = 16;
struct Pulse {
    int clientX = 0;
    int clientY = 0;
    RunPointerFeedbackKind kind = RunPointerFeedbackKind::MatchMiss;
    ULONGLONG startMs = 0;
    ClickPointerFeedbackSettings clickSettings{};
    bool useCustomFeedbackStyle = false;
};

struct OverlayState {
    HWND hwnd = nullptr;
    std::uint64_t token=0;
    QRect physicalBounds;
    std::vector<Pulse> pulses;
};

std::map<std::uint64_t,std::unique_ptr<OverlayState>> g_states;
PointerFeedbackLayers g_layers;
FeedbackCaptureBarrier g_capture;

ULONGLONG nowMs() {return GetTickCount64();}
bool ownsOverlayWindow(HWND hwnd,std::uint64_t token) {
    DWORD process=0;wchar_t name[128]{};
    if(!hwnd || !GetWindowThreadProcessId(hwnd,&process) || process!=GetCurrentProcessId())return false;
    const bool matches=GetClassNameW(hwnd,name,128)>0 && std::wstring(name)==kOverlayClassName;
    return ownedPointerFeedbackWindow(process,GetCurrentProcessId(),token,
        static_cast<std::uint64_t>(GetWindowLongPtrW(hwnd,GWLP_USERDATA)),matches);
}
void destroyOverlayWindow(std::uint64_t token) {
    const auto it=g_states.find(token);
    if(it!=g_states.end()) {
        const auto hwnd=it->second->hwnd;
        if(ownsOverlayWindow(hwnd,token)){it->second->hwnd=nullptr;KillTimer(hwnd,kTimerId);DestroyWindow(hwnd);}
        g_states.erase(it);
    }
    g_layers.retire(token);
}
OverlayState* stateFor(HWND hwnd) {
    for(auto& item:g_states)if(item.second->hwnd==hwnd)return item.second.get();return nullptr;
}

ULONGLONG pulseLifetimeMs(const Pulse& pulse) {
    if (pulse.useCustomFeedbackStyle) {
        return static_cast<ULONGLONG>(pulse.clickSettings.displayDurationMs);
    }
    if (pulse.kind == RunPointerFeedbackKind::Click) {
        return static_cast<ULONGLONG>(PointerFeedbackSettings::click().displayDurationMs);
    }
    if (pulse.kind == RunPointerFeedbackKind::TriggerScan) {
        return kTriggerScanPulseLifetimeMs;
    }
    return kDefaultMatchPulseLifetimeMs;
}

void pruneExpiredPulses(OverlayState* state, ULONGLONG currentMs) {
    if (!state) {
        return;
    }
    auto& pulses = state->pulses;
    pulses.erase(std::remove_if(pulses.begin(),
                                pulses.end(),
                                [currentMs](const Pulse& pulse) {
                                    return currentMs - pulse.startMs > pulseLifetimeMs(pulse);
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

void renderTriggerScanPulse(uint32_t* pixels,
                            int width,
                            int height,
                            const Pulse& pulse,
                            ULONGLONG age,
                            ULONGLONG lifetimeMs) {
    const float t = static_cast<float>(age) / static_cast<float>(lifetimeMs);
    const float fade = 1.0f - t;
    const int centerX = pulse.clientX;
    const int centerY = pulse.clientY;
    const uint8_t r = 88;
    const uint8_t g = 172;
    const uint8_t b = 156;

    const uint8_t coreAlpha = static_cast<uint8_t>(std::clamp(fade * 52.0f, 0.0f, 52.0f));
    fillCircle(pixels, width, height, centerX, centerY, 3, coreAlpha, r, g, b);

    const int radius = 5 + static_cast<int>(t * 28.0f);
    const uint8_t ringAlpha = static_cast<uint8_t>(std::clamp((1.0f - t) * 88.0f, 0.0f, 88.0f));
    strokeRing(pixels, width, height, centerX, centerY, radius, 2, ringAlpha, r, g, b);

    const float lagT = std::clamp((age - 180ULL) / static_cast<float>(lifetimeMs), 0.0f, 1.0f);
    if (lagT > 0.0f) {
        const int lagRadius = 7 + static_cast<int>(lagT * 34.0f);
        const uint8_t lagAlpha = static_cast<uint8_t>(std::clamp((1.0f - lagT) * 58.0f, 0.0f, 58.0f));
        strokeRing(pixels, width, height, centerX, centerY, lagRadius, 2, lagAlpha, r, g, b);
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
    const bool success = pulse.kind == RunPointerFeedbackKind::MatchSuccess;
    const QColor coreColor = pulse.useCustomFeedbackStyle && pulse.kind != RunPointerFeedbackKind::Click
                                 ? detectionCoreColor(pulse.clickSettings, success)
                                 : QColor();
    const QColor ringColor = pulse.useCustomFeedbackStyle && pulse.kind != RunPointerFeedbackKind::Click
                                 ? detectionRingColor(pulse.clickSettings, success)
                                 : QColor();
    renderClickPointerFeedbackFrame(pixels,
                                    width,
                                    height,
                                    pulse.clientX,
                                    pulse.clientY,
                                    age,
                                    lifetimeMs,
                                    pulse.clickSettings,
                                    coreColor,
                                    ringColor);
}

int pulseRenderMarginPx(const Pulse& pulse) {
    if (pulse.useCustomFeedbackStyle
        || pulse.kind == RunPointerFeedbackKind::Click) {
        const auto& settings =
            pulse.useCustomFeedbackStyle ? pulse.clickSettings : PointerFeedbackSettings::click();
        return settings.maxExpandRadius + settings.coreSize + settings.ringThickness + 12;
    }
    if (pulse.kind == RunPointerFeedbackKind::TriggerScan) {
        return 56;
    }
    return 40;
}

QRect computeActivePulseClientRegion(const OverlayState* state) {
    if (!state || state->pulses.empty()) {
        return {};
    }

    const int windowW = state->physicalBounds.width();
    const int windowH = state->physicalBounds.height();
    if (windowW <= 0 || windowH <= 0) {
        return {};
    }

    const ULONGLONG currentMs = nowMs();
    int minX = windowW;
    int minY = windowH;
    int maxX = 0;
    int maxY = 0;
    bool any = false;

    for (const Pulse& pulse : state->pulses) {
        const ULONGLONG lifetime = pulseLifetimeMs(pulse);
        const ULONGLONG age = currentMs - pulse.startMs;
        if (age > lifetime) {
            continue;
        }

        const int margin = pulseRenderMarginPx(pulse);
        minX = std::min(minX, pulse.clientX - margin);
        minY = std::min(minY, pulse.clientY - margin);
        maxX = std::max(maxX, pulse.clientX + margin);
        maxY = std::max(maxY, pulse.clientY + margin);
        any = true;
    }

    if (!any) {
        return {};
    }

    minX = std::max(0, minX);
    minY = std::max(0, minY);
    maxX = std::min(windowW - 1, maxX);
    maxY = std::min(windowH - 1, maxY);
    if (maxX < minX || maxY < minY) {
        return {};
    }

    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

void renderOverlayFrame(OverlayState* state) {
    if (!state || !ownsOverlayWindow(state->hwnd,state->token)) {
        return;
    }

    const QRect clientRegion = computeActivePulseClientRegion(state);
    if (clientRegion.isEmpty()) {
        return;
    }

    const int width = clientRegion.width();
    const int height = clientRegion.height();
    if (width <= 0 || height <= 0) {
        return;
    }

    const int regionOffsetX = clientRegion.x();
    const int regionOffsetY = clientRegion.y();
    const QRect screenRegion = clientRegion.translated(state->physicalBounds.topLeft());

    SetWindowPos(state->hwnd,
                 HWND_TOPMOST,
                 screenRegion.x(),
                 screenRegion.y(),
                 screenRegion.width(),
                 screenRegion.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

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
    for (const Pulse& pulse : state->pulses) {
        const ULONGLONG lifetime = pulseLifetimeMs(pulse);
        const ULONGLONG age = currentMs - pulse.startMs;
        if (age > lifetime) {
            continue;
        }

        Pulse localPulse = pulse;
        localPulse.clientX -= regionOffsetX;
        localPulse.clientY -= regionOffsetY;

        if (pulse.kind == RunPointerFeedbackKind::Click) {
            renderClickPulse(pixels, width, height, localPulse, age, lifetime);
        } else if (pulse.kind == RunPointerFeedbackKind::TriggerScan) {
            renderTriggerScanPulse(pixels, width, height, localPulse, age, lifetime);
        } else if (pulse.useCustomFeedbackStyle) {
            renderClickPulse(pixels, width, height, localPulse, age, lifetime);
        } else {
            renderMatchPulse(pixels, width, height, localPulse, age, lifetime);
        }
    }

    POINT ptDst{screenRegion.x(), screenRegion.y()};
    SIZE size{width, height};
    POINT ptSrc{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(state->hwnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if(message==kHideForCaptureMsg) {
        return g_capture.synchronized([&]() -> LRESULT {
            auto* state=stateFor(hwnd);if(!state || !ownsOverlayWindow(hwnd,static_cast<std::uint64_t>(wParam)))return 0;
            state->pulses.clear();ShowWindow(hwnd,SW_HIDE);return 1;
        });
    }
    if(message==WM_TIMER && wParam==kTimerId) {
        g_capture.publish([&]{
            auto* state=stateFor(hwnd);if(!state)return;
            if(!ownsOverlayWindow(hwnd,state->token)){destroyOverlayWindow(state->token);return;}
            pruneExpiredPulses(state,nowMs());
            if(state->pulses.empty()){destroyOverlayWindow(state->token);return;}
            renderOverlayFrame(state);
        });
        return 0;
    }
    if(message==WM_DESTROY){KillTimer(hwnd,kTimerId);return 0;}
    if(message==WM_NCDESTROY) {
        g_capture.synchronized([&]{
            if(auto* state=stateFor(hwnd)) {
                const auto token=state->token;state->hwnd=nullptr;
                if(auto* app=QCoreApplication::instance())QMetaObject::invokeMethod(app,[token]{
                    g_capture.synchronized([&]{destroyOverlayWindow(token);});
                },Qt::QueuedConnection);
            }
        });
    }
    return DefWindowProcW(hwnd,message,wParam,lParam);
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

OverlayState* ensureOverlayWindow(PointerFeedbackOwner owner,const PointerFeedbackFrame& frame) {
    const auto update=g_layers.observe(std::move(owner),frame);
    for(auto token:update.retired)destroyOverlayWindow(token);
    if(!update.token)return nullptr;
    const auto it=g_states.find(update.token);
    if(it!=g_states.end()) {
        if(ownsOverlayWindow(it->second->hwnd,update.token))return it->second.get();
        auto prior=g_layers.find(update.token)->owner;destroyOverlayWindow(update.token);
        return ensureOverlayWindow(std::move(prior),frame);
    }
    if(!ensureOverlayClassRegistered()){g_layers.retire(update.token);return nullptr;}
    auto value=std::make_unique<OverlayState>();value->token=update.token;value->physicalBounds=frame.clientBounds;
    value->hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,
        kOverlayClassName,L"",WS_POPUP,frame.clientBounds.x(),frame.clientBounds.y(),1,1,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!value->hwnd){g_layers.retire(update.token);return nullptr;}
    SetWindowLongPtrW(value->hwnd,GWLP_USERDATA,static_cast<LONG_PTR>(update.token));
    if(!ownsOverlayWindow(value->hwnd,update.token)){DestroyWindow(value->hwnd);g_layers.retire(update.token);return nullptr;}
    auto* state=value.get();g_states.emplace(update.token,std::move(value));
    SetTimer(state->hwnd,kTimerId,kTimerMs,nullptr);
    return state; // renderOverlayFrame shows only the bounded pulse region, without activation.
}

#endif // _WIN32

} // namespace

std::optional<PointerFeedbackFrame> WorkflowMatchFeedbackOverlay::frameForWindow(quintptr window) {
#ifdef _WIN32
    const auto hwnd=reinterpret_cast<HWND>(window);RECT client{};POINT origin{};DWORD process=0;
    if(!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)
        || !GetWindowThreadProcessId(hwnd,&process) || !GetClientRect(hwnd,&client) || !ClientToScreen(hwnd,&origin))return {};
    PointerFeedbackFrame frame{window,process,QRect(origin.x,origin.y,client.right-client.left,client.bottom-client.top)};
    if(frame.valid())return frame;
#else
    (void)window;
#endif
    return {};
}

WorkflowMatchFeedbackOverlay::CaptureLease WorkflowMatchFeedbackOverlay::prepareCapture() {
#ifdef _WIN32
    auto captured=g_capture.capture([] {
        std::vector<FeedbackHideRequest> windows;
        for(const auto& item:g_states)windows.push_back({reinterpret_cast<quintptr>(item.second->hwnd),item.first});return windows;
    });
    const bool ready=hideFeedbackBatch(captured.second,{[]{return GetTickCount64();},
        [](quintptr window){const auto hwnd=reinterpret_cast<HWND>(window);
            return ownsOverlayWindow(hwnd,static_cast<std::uint64_t>(GetWindowLongPtrW(hwnd,GWLP_USERDATA)));},
        [](const FeedbackHideRequest& request,unsigned timeout){DWORD_PTR response=0;
            if(!ownsOverlayWindow(reinterpret_cast<HWND>(request.window),request.token))return false;
            return SendMessageTimeoutW(reinterpret_cast<HWND>(request.window),kHideForCaptureMsg,static_cast<WPARAM>(request.token),0,
                SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,timeout,&response)!=0 && response==1;}});
    return {std::move(captured.first),ready};
#else
    return {{},true};
#endif
}

void WorkflowMatchFeedbackOverlay::pulseAtFrame(PointerFeedbackOwner owner,const PointerFeedbackFrame& frame,int x,int y,
    RunPointerFeedbackKind kind,const std::optional<ClickPointerFeedbackSettings>& feedback) {
#ifdef _WIN32
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1 || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1
        || !QCoreApplication::instance() || QThread::currentThread()!=QCoreApplication::instance()->thread())return;
    if(!frame.valid() || x<0 || y<0 || x>=frame.clientBounds.width() || y>=frame.clientBounds.height())return;
    g_capture.publish([&]{
        auto* state=ensureOverlayWindow(std::move(owner),frame);if(!state)return;
        pruneExpiredPulses(state,nowMs());
        if(kind==RunPointerFeedbackKind::TriggerScan || (kind==RunPointerFeedbackKind::Click && feedback)) {
            for(auto& pulse:state->pulses)if(pulse.kind==kind) {
                pulse.clientX=x;pulse.clientY=y;pulse.startMs=nowMs();
                pulse.useCustomFeedbackStyle=feedback.has_value();if(feedback)pulse.clickSettings=*feedback;
                renderOverlayFrame(state);return;
            }
        }
        Pulse pulse;pulse.clientX=x;pulse.clientY=y;pulse.kind=kind;pulse.startMs=nowMs();
        pulse.useCustomFeedbackStyle=feedback.has_value();if(feedback)pulse.clickSettings=*feedback;
        state->pulses.push_back(pulse);
        if(state->pulses.size()>kMaxActivePulses)state->pulses.erase(state->pulses.begin());
        renderOverlayFrame(state);
    });
#else
    (void)owner;(void)frame;(void)x;(void)y;(void)kind;(void)feedback;
#endif
}

void WorkflowMatchFeedbackOverlay::pulseAtClientPoint(int x,int y,RunPointerFeedbackKind kind) {
#ifdef _WIN32
    const auto frame=frameForWindow(reinterpret_cast<quintptr>(ScreenCapture::findTargetWindow()));
    if(frame)pulseAtFrame({QString(),"editor-preview",{}},*frame,x,y,kind);
#else
    (void)x;(void)y;(void)kind;
#endif
}
void WorkflowMatchFeedbackOverlay::pulseAtClientPoint(int x,int y,RunPointerFeedbackKind kind,const ClickPointerFeedbackSettings& feedback) {
#ifdef _WIN32
    const auto frame=frameForWindow(reinterpret_cast<quintptr>(ScreenCapture::findTargetWindow()));
    if(frame)pulseAtFrame({QString(),"editor-preview",{}},*frame,x,y,kind,feedback);
#else
    (void)x;(void)y;(void)kind;(void)feedback;
#endif
}
void WorkflowMatchFeedbackOverlay::dismissAll() {
#ifdef _WIN32
    g_capture.synchronized([]{while(!g_states.empty())destroyOverlayWindow(g_states.begin()->first);});
#endif
}
