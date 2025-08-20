#pragma once

#include "crgb.h"
#include "fl/colorutils_misc.h"
#include "fl/int.h"
#include "fl/stdint.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION

/// ANSI: signed short _Accum.  8 bits int, 7 bits fraction
/// @see accum88
#define saccum87 i16

namespace fl {

/// @defgroup ColorUtils Color Utility Functions
/// A variety of functions for working with color, palettes, and leds
/// @{

/// @defgroup ColorFills Color Fill Functions
/// Functions for filling LED arrays with colors and gradients
/// @{

/// Fill a range of LEDs with a solid color.
/// @param targetArray a pointer to the LED array to fill
/// @param numToFill the number of LEDs to fill in the array
/// @param color the color to fill with
void fill_solid(struct CRGB *targetArray, int numToFill,
                const struct CRGB &color);

/// @copydoc fill_solid()
void fill_solid(struct CHSV *targetArray, int numToFill,
                const struct CHSV &color);

/// Fill a range of LEDs with a rainbow of colors.
/// The colors making up the rainbow are at full saturation and full
/// value (brightness).
/// @param targetArray a pointer to the LED array to fill
/// @param numToFill the number of LEDs to fill in the array
/// @param initialhue the starting hue for the rainbow
/// @param deltahue how many hue values to advance for each LED
void fill_rainbow(struct CRGB *targetArray, int numToFill, fl::u8 initialhue,
                  fl::u8 deltahue = 5);

/// @copydoc fill_rainbow()
void fill_rainbow(struct CHSV *targetArray, int numToFill, fl::u8 initialhue,
                  fl::u8 deltahue = 5);

/// Fill a range of LEDs with a rainbow of colors, so that the hues
/// are continuous between the end of the strip and the beginning.
/// The colors making up the rainbow are at full saturation and full
/// value (brightness).
/// @param targetArray a pointer to the LED array to fill
/// @param numToFill the number of LEDs to fill in the array
/// @param initialhue the starting hue for the rainbow
/// @param reversed whether to progress through the rainbow hues backwards
void fill_rainbow_circular(struct CRGB *targetArray, int numToFill,
                           fl::u8 initialhue, bool reversed = false);

/// @copydoc fill_rainbow_circular()
void fill_rainbow_circular(struct CHSV *targetArray, int numToFill,
                           fl::u8 initialhue, bool reversed = false);

/// Fill a range of LEDs with a smooth HSV gradient between two HSV colors.
/// This function can write the gradient colors either:
///
///   1. Into an array of CRGBs (e.g., an leds[] array, or a CRGB palette)
///   2. Into an array of CHSVs (e.g. a CHSV palette).
///
/// In the case of writing into a CRGB array, the gradient is
/// computed in HSV space, and then HSV values are converted to RGB
/// as they're written into the CRGB array.
/// @param targetArray a pointer to the color array to fill
/// @param startpos the starting position in the array
/// @param startcolor the starting color for the gradient
/// @param endpos the ending position in the array
/// @param endcolor the end color for the gradient
/// @param directionCode the direction to travel around the color wheel
template <typename T>
void fill_gradient(T *targetArray, u16 startpos, CHSV startcolor,
                   u16 endpos, CHSV endcolor,
                   TGradientDirectionCode directionCode = SHORTEST_HUES) {
    // if the points are in the wrong order, straighten them
    if (endpos < startpos) {
        u16 t = endpos;
        CHSV tc = endcolor;
        endcolor = startcolor;
        endpos = startpos;
        startpos = t;
        startcolor = tc;
    }

    // If we're fading toward black (val=0) or white (sat=0),
    // then set the endhue to the starthue.
    // This lets us ramp smoothly to black or white, regardless
    // of what 'hue' was set in the endcolor (since it doesn't matter)
    if (endcolor.value == 0 || endcolor.saturation == 0) {
        endcolor.hue = startcolor.hue;
    }

    // Similarly, if we're fading in from black (val=0) or white (sat=0)
    // then set the starthue to the endhue.
    // This lets us ramp smoothly up from black or white, regardless
    // of what 'hue' was set in the startcolor (since it doesn't matter)
    if (startcolor.value == 0 || startcolor.saturation == 0) {
        startcolor.hue = endcolor.hue;
    }

    saccum87 huedistance87;
    saccum87 satdistance87;
    saccum87 valdistance87;

    satdistance87 = (endcolor.sat - startcolor.sat) << 7;
    valdistance87 = (endcolor.val - startcolor.val) << 7;

    fl::u8 huedelta8 = endcolor.hue - startcolor.hue;

    if (directionCode == SHORTEST_HUES) {
        directionCode = FORWARD_HUES;
        if (huedelta8 > 127) {
            directionCode = BACKWARD_HUES;
        }
    }

    if (directionCode == LONGEST_HUES) {
        directionCode = FORWARD_HUES;
        if (huedelta8 < 128) {
            directionCode = BACKWARD_HUES;
        }
    }

    if (directionCode == FORWARD_HUES) {
        huedistance87 = huedelta8 << 7;
    } else /* directionCode == BACKWARD_HUES */
    {
        huedistance87 = (fl::u8)(256 - huedelta8) << 7;
        huedistance87 = -huedistance87;
    }

    u16 pixeldistance = endpos - startpos;
    i16 divisor = pixeldistance ? pixeldistance : 1;

#if FASTLED_USE_32_BIT_GRADIENT_FILL
    // Use higher precision 32 bit math for new micros.
    i32 huedelta823 = (huedistance87 * 65536) / divisor;
    i32 satdelta823 = (satdistance87 * 65536) / divisor;
    i32 valdelta823 = (valdistance87 * 65536) / divisor;

    huedelta823 *= 2;
    satdelta823 *= 2;
    valdelta823 *= 2;
    u32 hue824 = static_cast<u32>(startcolor.hue) << 24;
    u32 sat824 = static_cast<u32>(startcolor.sat) << 24;
    u32 val824 = static_cast<u32>(startcolor.val) << 24;
    for (u16 i = startpos; i <= endpos; ++i) {
        targetArray[i] = CHSV(hue824 >> 24, sat824 >> 24, val824 >> 24);
        hue824 += huedelta823;
        sat824 += satdelta823;
        val824 += valdelta823;
    }
#else
    // Use 8-bit math for older micros.
    saccum87 huedelta87 = huedistance87 / divisor;
    saccum87 satdelta87 = satdistance87 / divisor;
    saccum87 valdelta87 = valdistance87 / divisor;

    huedelta87 *= 2;
    satdelta87 *= 2;
    valdelta87 *= 2;

    accum88 hue88 = startcolor.hue << 8;
    accum88 sat88 = startcolor.sat << 8;
    accum88 val88 = startcolor.val << 8;
    for (u16 i = startpos; i <= endpos; ++i) {
        targetArray[i] = CHSV(hue88 >> 8, sat88 >> 8, val88 >> 8);
        hue88 += huedelta87;
        sat88 += satdelta87;
        val88 += valdelta87;
    }
#endif // defined(__AVR__)
}

/// Fill a range of LEDs with a smooth HSV gradient between two HSV colors.
/// @see fill_gradient()
/// @param targetArray a pointer to the color array to fill
/// @param numLeds the number of LEDs to fill
/// @param c1 the starting color in the gradient
/// @param c2 the end color for the gradient
/// @param directionCode the direction to travel around the color wheel
template <typename T>
void fill_gradient(T *targetArray, u16 numLeds, const CHSV &c1,
                   const CHSV &c2,
                   TGradientDirectionCode directionCode = SHORTEST_HUES) {
    u16 last = numLeds - 1;
    fill_gradient(targetArray, 0, c1, last, c2, directionCode);
}

/// Fill a range of LEDs with a smooth HSV gradient between three HSV colors.
/// @see fill_gradient()
/// @param targetArray a pointer to the color array to fill
/// @param numLeds the number of LEDs to fill
/// @param c1 the starting color in the gradient
/// @param c2 the middle color for the gradient
/// @param c3 the end color for the gradient
/// @param directionCode the direction to travel around the color wheel
template <typename T>
void fill_gradient(T *targetArray, u16 numLeds, const CHSV &c1,
                   const CHSV &c2, const CHSV &c3,
                   TGradientDirectionCode directionCode = SHORTEST_HUES) {
    u16 half = (numLeds / 2);
    u16 last = numLeds - 1;
    fill_gradient(targetArray, 0, c1, half, c2, directionCode);
    fill_gradient(targetArray, half, c2, last, c3, directionCode);
}

/// Fill a range of LEDs with a smooth HSV gradient between four HSV colors.
/// @see fill_gradient()
/// @param targetArray a pointer to the color array to fill
/// @param numLeds the number of LEDs to fill
/// @param c1 the starting color in the gradient
/// @param c2 the first middle color for the gradient
/// @param c3 the second middle color for the gradient
/// @param c4 the end color for the gradient
/// @param directionCode the direction to travel around the color wheel
template <typename T>
void fill_gradient(T *targetArray, u16 numLeds, const CHSV &c1,
                   const CHSV &c2, const CHSV &c3, const CHSV &c4,
                   TGradientDirectionCode directionCode = SHORTEST_HUES) {
    u16 onethird = (numLeds / 3);
    u16 twothirds = ((numLeds * 2) / 3);
    u16 last = numLeds - 1;
    fill_gradient(targetArray, 0, c1, onethird, c2, directionCode);
    fill_gradient(targetArray, onethird, c2, twothirds, c3, directionCode);
    fill_gradient(targetArray, twothirds, c3, last, c4, directionCode);
}

/// Convenience synonym
#define fill_gradient_HSV fill_gradient

/// Fill a range of LEDs with a smooth RGB gradient between two RGB colors.
/// Unlike HSV, there is no "color wheel" in RGB space, and therefore there's
/// only one "direction" for the gradient to go. This means there's no
/// TGradientDirectionCode parameter for direction.
/// @param leds a pointer to the LED array to fill
/// @param startpos the starting position in the array
/// @param startcolor the starting color for the gradient
/// @param endpos the ending position in the array
/// @param endcolor the end color for the gradient
void fill_gradient_RGB(CRGB *leds, u16 startpos, CRGB startcolor,
                       u16 endpos, CRGB endcolor);

/// Fill a range of LEDs with a smooth RGB gradient between two RGB colors.
/// @see fill_gradient_RGB()
/// @param leds a pointer to the LED array to fill
/// @param numLeds the number of LEDs to fill
/// @param c1 the starting color in the gradient
/// @param c2 the end color for the gradient
void fill_gradient_RGB(CRGB *leds, u16 numLeds, const CRGB &c1,
                       const CRGB &c2);

/// Fill a range of LEDs with a smooth RGB gradient between three RGB colors.
/// @see fill_gradient_RGB()
/// @param leds a pointer to the LED array to fill
/// @param numLeds the number of LEDs to fill
/// @param c1 the starting color in the gradient
/// @param c2 the middle color for the gradient
/// @param c3 the end color for the gradient
void fill_gradient_RGB(CRGB *leds, u16 numLeds, const CRGB &c1,
                       const CRGB &c2, const CRGB &c3);

/// Fill a range of LEDs with a smooth RGB gradient between four RGB colors.
/// @see fill_gradient_RGB()
/// @param leds a pointer to the LED array to fill
/// @param numLeds the number of LEDs to fill
/// @param c1 the starting color in the gradient
/// @param c2 the first middle color for the gradient
/// @param c3 the second middle color for the gradient
/// @param c4 the end color for the gradient
void fill_gradient_RGB(CRGB *leds, u16 numLeds, const CRGB &c1,
                       const CRGB &c2, const CRGB &c3, const CRGB &c4);

} // namespace fl

FL_DISABLE_WARNING_POP
