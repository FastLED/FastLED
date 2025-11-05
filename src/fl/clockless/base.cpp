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

} // namespace fl
