/// @file crgb.h
/// Legacy header. Prefer to use fl/rgb8.h instead.
/// Provides backward compatibility by aliasing fl::rgb8 as CRGB

#pragma once

#include "fl/rgb8.h"
#include "fl/namespace.h"
#include "chsv.h"
#include "hsv2rgb.h"

FASTLED_NAMESPACE_BEGIN

// Whether to allow HD_COLOR_MIXING
#ifndef FASTLED_HD_COLOR_MIXING
#ifdef __AVR__
// Saves some memory on these constrained devices.
#define FASTLED_HD_COLOR_MIXING 0
#else
#define FASTLED_HD_COLOR_MIXING 1
#endif  // __AVR__
#endif  // FASTLED_HD_COLOR_MIXING


// Backward compatibility: bring fl::rgb8 into global namespace as CRGB
using CRGB = fl::rgb8;



/// HSV conversion function selection based on compile-time defines
/// This allows users to configure which HSV conversion algorithm to use
/// by setting one of the following defines:
/// - FASTLED_HSV_CONVERSION_SPECTRUM: Use spectrum conversion
/// - FASTLED_HSV_CONVERSION_FULL_SPECTRUM: Use full spectrum conversion
/// - FASTLED_HSV_CONVERSION_RAINBOW: Use rainbow conversion (explicit)
/// - Default (no define): Use rainbow conversion (backward compatibility)



/// Array-based HSV conversion function selection using the same compile-time defines
/// @param phsv CHSV array to convert to RGB
/// @param prgb CRGB array to store the result of the conversion (will be modified)
/// @param numLeds the number of array values to process
FASTLED_FORCE_INLINE void hsv2rgb_dispatch( const CHSV* phsv, CRGB * prgb, int numLeds)
{
#if defined(FASTLED_HSV_CONVERSION_SPECTRUM)
    hsv2rgb_spectrum(phsv, prgb, numLeds);
#elif defined(FASTLED_HSV_CONVERSION_FULL_SPECTRUM)
    hsv2rgb_fullspectrum(phsv, prgb, numLeds);
#elif defined(FASTLED_HSV_CONVERSION_RAINBOW)
    hsv2rgb_rainbow(phsv, prgb, numLeds);
#else
    // Default to rainbow for backward compatibility
    hsv2rgb_rainbow(phsv, prgb, numLeds);
#endif
}

FASTLED_FORCE_INLINE void hsv2rgb_dispatch( const CHSV& hsv, CRGB& rgb)
{
#if defined(FASTLED_HSV_CONVERSION_SPECTRUM)
    hsv2rgb_spectrum(hsv, rgb);
#elif defined(FASTLED_HSV_CONVERSION_FULL_SPECTRUM)
    hsv2rgb_fullspectrum(hsv, rgb);
#elif defined(FASTLED_HSV_CONVERSION_RAINBOW)
    hsv2rgb_rainbow(hsv, rgb);
#else
    hsv2rgb_rainbow(hsv, rgb);
#endif
}

FASTLED_NAMESPACE_END

// HSV conversion implementations for fl::rgb8
// These must be defined after CRGB and CHSV typedefs are established
// and after hsv2rgb_dispatch is defined
namespace fl {

FASTLED_FORCE_INLINE rgb8::rgb8(const hsv8& rhs) {
    hsv2rgb_dispatch(rhs, *this);
}

FASTLED_FORCE_INLINE rgb8& rgb8::setHSV(u8 hue, u8 sat, u8 val) {
    hsv2rgb_dispatch(hsv8(hue, sat, val), *this);
    return *this;
}

FASTLED_FORCE_INLINE rgb8& rgb8::setHue(u8 hue) {
    hsv2rgb_dispatch(hsv8(hue, 255, 255), *this);
    return *this;
}

FASTLED_FORCE_INLINE rgb8& rgb8::operator=(const hsv8& rhs) {
    hsv2rgb_dispatch(rhs, *this);
    return *this;
}

}  // namespace fl
