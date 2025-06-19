#pragma once

#include "fl/stdint.h"

namespace fl {

// Struct to hold CIELAB values as 16-bit unsigned integers
struct CIELAB_16 {
    uint16_t L;  // Lightness
    uint16_t A;  // Green-Red axis
    uint16_t B;  // Blue-Yellow axis
    
    // Constructor
    CIELAB_16(uint16_t l = 0, uint16_t a = 0, uint16_t b = 0) 
        : L(l), A(a), B(b) {}
};


// Convenience function: sRGB_u8 → CIELAB_16 struct
CIELAB_16 rgb_to_cielab_16(uint8_t r, uint8_t g, uint8_t b);

} // namespace fl