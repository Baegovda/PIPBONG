#pragma once

#include "app/PointerFeedbackSettings.h"

#include <cstdint>

/// Renders one frame of the click pointer-feedback animation into a 32-bit ARGB buffer.
void renderClickPointerFeedbackFrame(uint32_t* pixels,
                                     int width,
                                     int height,
                                     int centerX,
                                     int centerY,
                                     uint64_t ageMs,
                                     uint64_t lifetimeMs,
                                     const ClickPointerFeedbackSettings& settings);
