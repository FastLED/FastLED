/// @file crgb.cpp
/// Utility functions for the red, green, and blue (RGB) pixel struct

#define FASTLED_INTERNAL
#include "FastLED.h"
#include "crgb.h"
#include "lib8tion/math8.h"

#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

fl::Str CRGB::toString() const {
    fl::Str out;
    out.append("CRGB(");
    out.append(int16_t(r));
    out.append(",");
    out.append(int16_t(g));
    out.append(",");
    out.append(int16_t(b));
    out.append(")");
    return out;
}

CRGB CRGB::computeAdjustment(uint8_t scale, const CRGB & colorCorrection, const CRGB & colorTemperature) {
    #if defined(NO_CORRECTION) && (NO_CORRECTION==1)
            return CRGB(scale,scale,scale);
    #else
            CRGB adj(0,0,0);
            if(scale > 0) {
                for(uint8_t i = 0; i < 3; ++i) {
                    uint8_t cc = colorCorrection.raw[i];
                    uint8_t ct = colorTemperature.raw[i];
                    if(cc > 0 && ct > 0) {
                        // Optimized for AVR size. This function is only called very infrequently so size
                        // matters more than speed.
                        uint32_t work = (((uint16_t)cc)+1);
                        work *= (((uint16_t)ct)+1);
                        work *= scale;
                        work /= 0x10000L;
                        adj.raw[i] = work & 0xFF;
                    }
                }
            }
            return adj;
    #endif
}


CRGB CRGB::blend(const CRGB& p1, const CRGB& p2, fract8 amountOfP2) {
    return CRGB(
        blend8(p1.r, p2.r, amountOfP2),
        blend8(p1.g, p2.g, amountOfP2),
        blend8(p1.b, p2.b, amountOfP2)
    );
}

CRGB& CRGB::nscale8 (uint8_t scaledown )
{
    nscale8x3( r, g, b, scaledown);
    return *this;
}


FASTLED_NAMESPACE_END
