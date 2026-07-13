#pragma once

#include "app/PointerFeedbackSettings.h"

#include <cstdint>

/// Renders one frame of the window-pick confirmation animation into a 32-bit ARGB buffer.
void renderWindowSelectionFeedbackFrame(uint32_t* pixels,
                                        int width,
                                        int height,
                                        float progress,
                                        const WindowSelectionFeedbackSettings& settings);
