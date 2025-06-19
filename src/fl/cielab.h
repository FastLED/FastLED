#pragma once

#include "fl/stdint.h"
#include "crgb.h"

namespace fl {

// Struct to hold CIELAB values as 16-bit unsigned integers
struct CIELAB16 {
    uint16_t l;  // Lightness
    uint16_t a;  // Green-Red axis
    uint16_t b;  // Blue-Yellow axis
    
    // Constructor
    CIELAB16(uint16_t l = 0, uint16_t a = 0, uint16_t b = 0) 
        : l(l), a(a), b(b) {}
    // Conversion from CRGB
    CIELAB16(const CRGB& c);
    CRGB ToRGB() const;

    CRGB ToVideoRGB() const;

    static void Fill(const CRGB* c, CIELAB16* lab, size_t numLeds);
    static void Fill(const CIELAB16* c, CRGB* lab, size_t numLeds);
};


} // namespace fl