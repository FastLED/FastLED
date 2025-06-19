#pragma once

#include "fl/stdint.h"
#include "crgb.h"

namespace fl {

// Struct to hold CIELAB values as 16-bit unsigned integers
struct CIELAB16 {
    uint16_t L;  // Lightness
    uint16_t A;  // Green-Red axis
    uint16_t B;  // Blue-Yellow axis
    
    // Constructor
    CIELAB16(uint16_t l = 0, uint16_t a = 0, uint16_t b = 0) 
        : L(l), A(a), B(b) {}
    // Conversion from CRGB
    CIELAB16(const CRGB& c);
    CRGB ToRGB() const;

    static void Fill(const CRGB* c, CIELAB16* lab, int numLeds);
    static void Fill(const CIELAB16* c, CRGB* lab, int numLeds);
};


} // namespace fl