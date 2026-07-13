#include "ui/WindowSelectionFeedbackRenderer.h"

#include <QColor>
#include <algorithm>
#include <cmath>

namespace {

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

void drawCircleFill(uint32_t* pixels,
                    int width,
                    int height,
                    float radius,
                    float edgeWidth,
                    uint8_t fillAlpha,
                    uint8_t edgeAlpha,
                    uint8_t r,
                    uint8_t g,
                    uint8_t b) {
    const float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const int top = std::max(0, static_cast<int>(std::floor(cy - radius - edgeWidth)));
    const int bottom = std::min(height - 1, static_cast<int>(std::ceil(cy + radius + edgeWidth)));

    const float outerRadius = radius + edgeWidth;
    const float outerSq = outerRadius * outerRadius;
    const float innerEdgeStart = std::max(0.0f, radius - edgeWidth);
    const float innerEdgeSq = innerEdgeStart * innerEdgeStart;
    const float radiusSq = radius * radius;

    for (int y = top; y <= bottom; ++y) {
        const float dy = static_cast<float>(y) - cy;
        const float maxDxSq = outerSq - dy * dy;
        if (maxDxSq < 0.0f) {
            continue;
        }
        const int left = std::max(0, static_cast<int>(std::floor(cx - std::sqrt(maxDxSq))));
        const int right = std::min(width - 1, static_cast<int>(std::ceil(cx + std::sqrt(maxDxSq))));
        for (int x = left; x <= right; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float distSq = dx * dx + dy * dy;
            uint8_t alpha = 0;
            if (distSq <= radiusSq) {
                alpha = fillAlpha;
            }
            if (distSq >= innerEdgeSq && distSq <= outerSq) {
                alpha = std::max(alpha, edgeAlpha);
            }
            if (alpha > 0) {
                setPixelArgb(pixels, width, height, x, y, alpha, r, g, b);
            }
        }
    }
}

void drawCornerBrackets(uint32_t* pixels,
                        int width,
                        int height,
                        float slideOffset,
                        int bracketScalePercent,
                        uint8_t alpha,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b) {
    if (alpha == 0) {
        return;
    }
    const float scale = std::clamp(bracketScalePercent, 50, 150) / 100.0f;
    const int armLength =
        std::clamp(static_cast<int>(std::min(width, height) * 0.075f * scale), 16, 180);
    const int thickness =
        std::clamp(static_cast<int>(std::min(width, height) * 0.008f * scale), 3, 14);
    const int inset = std::clamp(static_cast<int>(std::min(width, height) * 0.02f * scale), 8, 56);

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
    const int lx = inset - slide;
    const int ty = inset - slide;
    const int rx = width - 1 - inset + slide;
    const int by = height - 1 - inset + slide;

    fillRect(lx, ty, lx + armLength, ty + thickness - 1);
    fillRect(lx, ty, lx + thickness - 1, ty + armLength);
    fillRect(rx - armLength, ty, rx, ty + thickness - 1);
    fillRect(rx - thickness + 1, ty, rx, ty + armLength);
    fillRect(lx, by - thickness + 1, lx + armLength, by);
    fillRect(lx, by - armLength, lx + thickness - 1, by);
    fillRect(rx - armLength, by - thickness + 1, rx, by);
    fillRect(rx - thickness + 1, by - armLength, rx, by);
}

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

void lighterRgb(uint8_t r, uint8_t g, uint8_t b, uint8_t& lr, uint8_t& lg, uint8_t& lb) {
    lr = static_cast<uint8_t>(std::min(255, r + (255 - r) * 35 / 100));
    lg = static_cast<uint8_t>(std::min(255, g + (255 - g) * 35 / 100));
    lb = static_cast<uint8_t>(std::min(255, b + (255 - b) * 35 / 100));
}

void renderLockOn(uint32_t* pixels,
                  int width,
                  int height,
                  float t,
                  const WindowSelectionFeedbackSettings& settings,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b) {
    const float minDim = static_cast<float>(std::min(width, height));
    const float maxRadius =
        std::sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height) * 0.5f;
    const float fadeOut = 1.0f - smoothstepf(0.82f, 1.0f, t);
    const float alphaScale = settings.maxAlpha / 220.0f;

    uint8_t lr = 0;
    uint8_t lg = 0;
    uint8_t lb = 0;
    lighterRgb(r, g, b, lr, lg, lb);

    const float ringT = std::clamp(t / 0.62f, 0.0f, 1.0f);
    const float ringRadius = maxRadius * easeOutExpo(ringT);
    const float ringSigma = std::max(8.0f, static_cast<float>(settings.pingRingWidth))
                            * (0.7f + 0.9f * ringT);
    const float ringFade = (1.0f - smoothstepf(0.72f, 1.0f, ringT)) * fadeOut;
    drawSoftRing(pixels, width, height, ringRadius, ringSigma, 205.0f * ringFade * alphaScale, r, g, b);

    if (settings.echoRing) {
        const float echoT = std::clamp((t - 0.10f) / 0.62f, 0.0f, 1.0f);
        if (echoT > 0.0f) {
            const float echoRadius = maxRadius * easeOutExpo(echoT) * 0.82f;
            const float echoFade = (1.0f - smoothstepf(0.65f, 1.0f, echoT)) * fadeOut;
            drawSoftRing(pixels, width, height, echoRadius,
                         std::max(6.0f, ringSigma * 0.5f), 90.0f * echoFade * alphaScale, lr, lg, lb);
        }
    }

    if (settings.centerBloom) {
        const float flashFade = 1.0f - smoothstepf(0.0f, 0.22f, t);
        if (flashFade > 0.0f) {
            const float flashRadius = minDim * 0.05f * (1.0f + 2.4f * smoothstepf(0.0f, 0.22f, t));
            drawSoftRing(pixels, width, height, flashRadius * 0.5f, flashRadius * 0.6f,
                         170.0f * flashFade * fadeOut * alphaScale, lr, lg, lb);
        }
    }

    if (settings.edgeGlow) {
        const float frameIn = smoothstepf(0.30f, 0.58f, t);
        if (frameIn > 0.0f) {
            drawEdgeGlow(pixels, width, height,
                         185.0f * frameIn * fadeOut * alphaScale,
                         70.0f * frameIn * fadeOut * alphaScale,
                         settings.edgeGlowWidth, r, g, b);
        }
    }

    if (settings.cornerBrackets) {
        const float bracketT = std::clamp((t - 0.42f) / 0.34f, 0.0f, 1.0f);
        if (bracketT > 0.0f) {
            const float slideMax = minDim * 0.05f;
            const float slide = slideMax * (1.0f - easeOutBack(bracketT));
            const uint8_t bracketAlpha = static_cast<uint8_t>(std::clamp(
                235.0f * smoothstepf(0.0f, 0.4f, bracketT) * fadeOut * alphaScale, 0.0f, 255.0f));
            drawCornerBrackets(pixels, width, height, slide, settings.bracketScalePercent, bracketAlpha,
                               lr, lg, lb);
        }
    }
}

void renderRadarPing(uint32_t* pixels,
                     int width,
                     int height,
                     float t,
                     const WindowSelectionFeedbackSettings& settings,
                     uint8_t r,
                     uint8_t g,
                     uint8_t b) {
    const float minDim = static_cast<float>(std::min(width, height));
    const float maxRadius =
        std::sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height) * 0.5f;
    const float fadeOut = 1.0f - smoothstepf(0.85f, 1.0f, t);
    const float alphaScale = settings.maxAlpha / 220.0f;

    uint8_t lr = 0;
    uint8_t lg = 0;
    uint8_t lb = 0;
    lighterRgb(r, g, b, lr, lg, lb);

    const float ringT = std::clamp(t / 0.75f, 0.0f, 1.0f);
    const float ringRadius = maxRadius * easeOutExpo(ringT);
    const float ringSigma = std::max(8.0f, static_cast<float>(settings.pingRingWidth));
    const float ringFade = (1.0f - smoothstepf(0.70f, 1.0f, ringT)) * fadeOut;
    drawSoftRing(pixels, width, height, ringRadius, ringSigma, 205.0f * ringFade * alphaScale, r, g, b);

    if (settings.echoRing) {
        const float echoT = std::clamp((t - 0.12f) / 0.75f, 0.0f, 1.0f);
        if (echoT > 0.0f) {
            const float echoRadius = maxRadius * easeOutExpo(echoT) * 0.78f;
            const float echoFade = (1.0f - smoothstepf(0.68f, 1.0f, echoT)) * fadeOut;
            drawSoftRing(pixels, width, height, echoRadius,
                         std::max(6.0f, ringSigma * 0.45f), 85.0f * echoFade * alphaScale, lr, lg, lb);
        }
    }

    if (settings.centerBloom) {
        const float flashFade = 1.0f - smoothstepf(0.0f, 0.18f, t);
        if (flashFade > 0.0f) {
            drawSoftRing(pixels, width, height, minDim * 0.04f, minDim * 0.03f,
                         120.0f * flashFade * fadeOut * alphaScale, lr, lg, lb);
        }
    }
}

void renderBorderGlowStyle(uint32_t* pixels,
                           int width,
                           int height,
                           float t,
                           const WindowSelectionFeedbackSettings& settings,
                           uint8_t r,
                           uint8_t g,
                           uint8_t b) {
    const float pulse = 0.5f * (1.0f + std::sin(t * 6.28318f * 1.5f));
    const float fadeOut = 1.0f - smoothstepf(0.88f, 1.0f, t);
    const float alphaScale = settings.maxAlpha / 220.0f;
    const float frameIn = smoothstepf(0.05f, 0.35f, t) * fadeOut;
    drawEdgeGlow(pixels, width, height,
                 (120.0f + 90.0f * pulse) * frameIn * alphaScale,
                 55.0f * frameIn * alphaScale,
                 settings.edgeGlowWidth, r, g, b);
    drawBorderFrame(pixels, width, height, 3,
                    static_cast<uint8_t>(std::clamp((80.0f + 60.0f * pulse) * frameIn * alphaScale,
                                                    0.0f, 255.0f)),
                    r, g, b);
}

void renderClassicFill(uint32_t* pixels,
                       int width,
                       int height,
                       float t,
                       const WindowSelectionFeedbackSettings& settings,
                       uint8_t r,
                       uint8_t g,
                       uint8_t b) {
    const float maxRadius =
        std::sqrt(static_cast<float>(width) * width + static_cast<float>(height) * height) * 0.5f;
    const float radius = maxRadius * std::clamp(t, 0.0f, 1.0f);
    const float lateFade = t > 0.72f ? (1.0f - t) / 0.28f : 1.0f;
    const float fade = std::clamp(lateFade, 0.0f, 1.0f);
    const float alphaScale = settings.maxAlpha / 220.0f;
    const float edgeWidth = std::max(12.0f, static_cast<float>(settings.pingRingWidth));
    const uint8_t fillAlpha =
        static_cast<uint8_t>(std::clamp(92.0f * fade * alphaScale, 0.0f, 255.0f));
    const uint8_t edgeAlpha =
        static_cast<uint8_t>(std::clamp(210.0f * fade * alphaScale, 0.0f, 255.0f));
    drawCircleFill(pixels, width, height, radius, edgeWidth, fillAlpha, edgeAlpha, r, g, b);
    drawBorderFrame(pixels, width, height, 4,
                    static_cast<uint8_t>(std::clamp(130.0f * fade * alphaScale, 0.0f, 255.0f)),
                    r, g, b);
}

} // namespace

void renderWindowSelectionFeedbackFrame(uint32_t* pixels,
                                        int width,
                                        int height,
                                        float progress,
                                        const WindowSelectionFeedbackSettings& settings) {
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    const float t = std::clamp(progress, 0.0f, 1.0f);
    const QColor color = settings.color.isValid() ? settings.color : QColor(56, 189, 248);
    const uint8_t r = static_cast<uint8_t>(color.red());
    const uint8_t g = static_cast<uint8_t>(color.green());
    const uint8_t b = static_cast<uint8_t>(color.blue());

    switch (settings.style) {
    case WindowSelectionFeedbackStyle::RadarPing:
        renderRadarPing(pixels, width, height, t, settings, r, g, b);
        break;
    case WindowSelectionFeedbackStyle::BorderGlow:
        renderBorderGlowStyle(pixels, width, height, t, settings, r, g, b);
        break;
    case WindowSelectionFeedbackStyle::ClassicFill:
        renderClassicFill(pixels, width, height, t, settings, r, g, b);
        break;
    case WindowSelectionFeedbackStyle::LockOn:
    default:
        renderLockOn(pixels, width, height, t, settings, r, g, b);
        break;
    }
}
