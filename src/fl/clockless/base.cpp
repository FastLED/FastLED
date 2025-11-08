/// @file fl/clockless/base.cpp
/// @brief Implementation file for BulkClockless helper functions

#include "base.h"

namespace fl {

ColorAdjustment BulkClocklessHelper::computeAdjustment(
    u8 brightness,
    const BulkStrip::Settings &settings) {
    CRGB scale = CRGB::computeAdjustment(brightness, settings.correction,
                                         settings.temperature);
#if FASTLED_HD_COLOR_MIXING
    CRGB color = CRGB::computeAdjustment(255, settings.correction,
                                         settings.temperature);
    return ColorAdjustment{scale, color, brightness};
#else
    return ColorAdjustment{scale};
#endif
}

void BulkClocklessHelper::applyAdjustmentToBuffer(CRGB* buffer, int count,
                                                   const ColorAdjustment& adj) {
#if FASTLED_HD_COLOR_MIXING
    // HD Color Mixing: Apply color correction and brightness separately
    // This provides 16-bit precision in intermediate calculations for smoother
    // color gradients and more accurate brightness scaling
    for (int i = 0; i < count; ++i) {
        // First apply color correction/temperature at full brightness
        buffer[i] = buffer[i].scale8(adj.color);
        // Then apply global brightness
        buffer[i].nscale8(adj.brightness);
    }
#else
    // Legacy mode: Use premixed brightness + color correction
    // This is the traditional 8-bit approach for backward compatibility
    for (int i = 0; i < count; ++i) {
        buffer[i].nscale8(adj.premixed);
    }
#endif
}

} // namespace fl
