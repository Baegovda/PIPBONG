#include "ui/ClickPointerFeedbackRenderer.h"

#include <algorithm>

namespace {

void setPixelArgb(uint32_t* bits, int width, int height, int x, int y, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    uint32_t& pixel = bits[y * width + x];
    const uint8_t invA = static_cast<uint8_t>(255 - a);
    const uint8_t existingA = static_cast<uint8_t>((pixel >> 24) & 0xff);
    const auto blend = [a, invA](uint8_t channel, uint8_t existing) -> uint8_t {
        return static_cast<uint8_t>((channel * a + existing * invA) / 255);
    };
    const uint8_t existingR = static_cast<uint8_t>((pixel >> 16) & 0xff);
    const uint8_t existingG = static_cast<uint8_t>((pixel >> 8) & 0xff);
    const uint8_t existingB = static_cast<uint8_t>(pixel & 0xff);
    const uint8_t outR = blend(r, existingR);
    const uint8_t outG = blend(g, existingG);
    const uint8_t outB = blend(b, existingB);
    pixel = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(outR) << 16)
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

void fillRect(uint32_t* bits,
              int width,
              int height,
              int left,
              int top,
              int right,
              int bottom,
              uint8_t a,
              uint8_t r,
              uint8_t g,
              uint8_t b) {
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            setPixelArgb(bits, width, height, x, y, a, r, g, b);
        }
    }
}

void strokeRectRing(uint32_t* bits,
                    int width,
                    int height,
                    int centerX,
                    int centerY,
                    int halfSize,
                    int thickness,
                    uint8_t a,
                    uint8_t r,
                    uint8_t g,
                    uint8_t b) {
    if (halfSize <= 0 || thickness <= 0) {
        return;
    }
    const int outerLeft = centerX - halfSize;
    const int outerTop = centerY - halfSize;
    const int outerRight = centerX + halfSize;
    const int outerBottom = centerY + halfSize;
    const int innerHalf = std::max(0, halfSize - thickness);
    const int innerLeft = centerX - innerHalf;
    const int innerTop = centerY - innerHalf;
    const int innerRight = centerX + innerHalf;
    const int innerBottom = centerY + innerHalf;

    for (int y = outerTop; y <= outerBottom; ++y) {
        for (int x = outerLeft; x <= outerRight; ++x) {
            const bool inOuter = x >= outerLeft && x <= outerRight && y >= outerTop && y <= outerBottom;
            const bool inInner =
                x >= innerLeft && x <= innerRight && y >= innerTop && y <= innerBottom;
            if (inOuter && !inInner) {
                setPixelArgb(bits, width, height, x, y, a, r, g, b);
            }
        }
    }
}

void strokeCrosshair(uint32_t* bits,
                     int width,
                     int height,
                     int centerX,
                     int centerY,
                     int armLength,
                     int thickness,
                     uint8_t a,
                     uint8_t r,
                     uint8_t g,
                     uint8_t b) {
    if (armLength <= 0 || thickness <= 0) {
        return;
    }
    const int half = thickness / 2;
    fillRect(bits,
             width,
             height,
             centerX - armLength,
             centerY - half,
             centerX + armLength,
             centerY + half,
             a,
             r,
             g,
             b);
    fillRect(bits,
             width,
             height,
             centerX - half,
             centerY - armLength,
             centerX + half,
             centerY + armLength,
             a,
             r,
             g,
             b);
}

} // namespace

void renderClickPointerFeedbackFrame(uint32_t* pixels,
                                     int width,
                                     int height,
                                     int centerX,
                                     int centerY,
                                     uint64_t ageMs,
                                     uint64_t lifetimeMs,
                                     const ClickPointerFeedbackSettings& settings,
                                     const QColor& coreColor,
                                     const QColor& ringColor) {
    if (!pixels || width <= 0 || height <= 0 || lifetimeMs <= 0) {
        return;
    }

    const QColor resolvedCore = coreColor.isValid() ? coreColor : resolvedClickCoreColor(settings);
    const QColor resolvedRing = ringColor.isValid() ? ringColor : resolvedClickRingColor(settings);

    const float t = static_cast<float>(ageMs) / static_cast<float>(lifetimeMs);
    const float fade = 1.0f - t;
    const uint8_t alpha =
        static_cast<uint8_t>(std::clamp(fade * static_cast<float>(settings.maxAlpha), 0.0f, 255.0f));
    const uint8_t coreR = static_cast<uint8_t>(resolvedCore.red());
    const uint8_t coreG = static_cast<uint8_t>(resolvedCore.green());
    const uint8_t coreB = static_cast<uint8_t>(resolvedCore.blue());
    const uint8_t ringR = static_cast<uint8_t>(resolvedRing.red());
    const uint8_t ringG = static_cast<uint8_t>(resolvedRing.green());
    const uint8_t ringB = static_cast<uint8_t>(resolvedRing.blue());

    const double speed = std::max(0.25, settings.animationSpeed);
    const uint64_t animAge = static_cast<uint64_t>(static_cast<double>(ageMs) * speed);

    switch (settings.shape) {
    case ClickPointerFeedbackShape::FilledDotRings:
        fillCircle(pixels, width, height, centerX, centerY, settings.coreSize, alpha, coreR, coreG, coreB);
        for (int ring = 0; ring < settings.ringCount; ++ring) {
            const float ringSpan = static_cast<float>(lifetimeMs) * 0.7f;
            const float ringDelay = 90.0f / static_cast<float>(speed);
            const float ringT = std::clamp((static_cast<float>(animAge)
                                            - static_cast<float>(ring) * ringDelay)
                                               / ringSpan,
                                           0.0f,
                                           1.0f);
            if (ringT <= 0.0f) {
                continue;
            }
            const int radius =
                settings.coreSize + 4 + static_cast<int>(ringT * static_cast<float>(settings.maxExpandRadius));
            const uint8_t ringAlpha =
                static_cast<uint8_t>(std::clamp((1.0f - ringT) * static_cast<float>(settings.maxAlpha) * 0.82f,
                                                0.0f,
                                                255.0f));
            strokeRing(pixels,
                       width,
                       height,
                       centerX,
                       centerY,
                       radius,
                       settings.ringThickness,
                       ringAlpha,
                       ringR,
                       ringG,
                       ringB);
        }
        break;
    case ClickPointerFeedbackShape::RingOnly:
        for (int ring = 0; ring < std::max(1, settings.ringCount); ++ring) {
            const float ringSpan = static_cast<float>(lifetimeMs) * 0.7f;
            const float ringDelay = 90.0f / static_cast<float>(speed);
            const float ringT = std::clamp((static_cast<float>(animAge)
                                            - static_cast<float>(ring) * ringDelay)
                                               / ringSpan,
                                           0.0f,
                                           1.0f);
            if (ringT <= 0.0f) {
                continue;
            }
            const int radius =
                settings.coreSize + static_cast<int>(ringT * static_cast<float>(settings.maxExpandRadius));
            const uint8_t ringAlpha =
                static_cast<uint8_t>(std::clamp((1.0f - ringT) * static_cast<float>(settings.maxAlpha), 0.0f, 255.0f));
            strokeRing(pixels,
                       width,
                       height,
                       centerX,
                       centerY,
                       radius,
                       settings.ringThickness,
                       ringAlpha,
                       ringR,
                       ringG,
                       ringB);
        }
        break;
    case ClickPointerFeedbackShape::Crosshair: {
        const float expandT = std::clamp(static_cast<float>(animAge) / (static_cast<float>(lifetimeMs) * 0.75f),
                                         0.0f,
                                         1.0f);
        const int armLength =
            settings.coreSize + static_cast<int>(expandT * static_cast<float>(settings.maxExpandRadius));
        strokeCrosshair(pixels,
                        width,
                        height,
                        centerX,
                        centerY,
                        armLength,
                        std::max(2, settings.ringThickness),
                        alpha,
                        ringR,
                        ringG,
                        ringB);
        break;
    }
    case ClickPointerFeedbackShape::Square:
        fillRect(pixels,
                 width,
                 height,
                 centerX - settings.coreSize,
                 centerY - settings.coreSize,
                 centerX + settings.coreSize,
                 centerY + settings.coreSize,
                 alpha,
                 coreR,
                 coreG,
                 coreB);
        for (int ring = 0; ring < settings.ringCount; ++ring) {
            const float ringSpan = static_cast<float>(lifetimeMs) * 0.7f;
            const float ringDelay = 90.0f / static_cast<float>(speed);
            const float ringT = std::clamp((static_cast<float>(animAge)
                                            - static_cast<float>(ring) * ringDelay)
                                               / ringSpan,
                                           0.0f,
                                           1.0f);
            if (ringT <= 0.0f) {
                continue;
            }
            const int halfSize =
                settings.coreSize + 2 + static_cast<int>(ringT * static_cast<float>(settings.maxExpandRadius));
            const uint8_t ringAlpha =
                static_cast<uint8_t>(std::clamp((1.0f - ringT) * static_cast<float>(settings.maxAlpha) * 0.82f,
                                                0.0f,
                                                255.0f));
            strokeRectRing(pixels,
                           width,
                           height,
                           centerX,
                           centerY,
                           halfSize,
                           settings.ringThickness,
                           ringAlpha,
                           ringR,
                           ringG,
                           ringB);
        }
        break;
    }
}
