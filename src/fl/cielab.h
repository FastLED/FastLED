#pragma once

#include "fl/stdint.h"

namespace fl {

// Main conversion: sRGB_u8 → CIELAB_u16
void rgb_to_lab_u16_fixed(
    uint8_t  r, uint8_t  g, uint8_t  b,
    uint16_t &outL, uint16_t &outA, uint16_t &outB
);

} // namespace fl