#define FASTLED_INTERNAL
#define __PROG_TYPES_COMPAT__

/// @file colorutils.cpp
/// Misc utility functions for palettes, blending, and more. This file is being
/// refactored.

#include <math.h>
#include "fl/stdint.h"

#include "FastLED.h"
#include "fl/assert.h"
#include "fl/colorutils.h"
#include "fl/unused.h"
#include "fl/xymap.h"

namespace fl {

CRGB &nblend(CRGB &existing, const CRGB &overlay, fract8 amountOfOverlay) {
    if (amountOfOverlay == 0) {
        return existing;
    }

    if (amountOfOverlay == 255) {
        existing = overlay;
        return existing;
    }

#if 0
    // Old blend method which unfortunately had some rounding errors
    fract8 amountOfKeep = 255 - amountOfOverlay;

    existing.red   = scale8_LEAVING_R1_DIRTY( existing.red,   amountOfKeep)
                    + scale8_LEAVING_R1_DIRTY( overlay.red,    amountOfOverlay);
    existing.green = scale8_LEAVING_R1_DIRTY( existing.green, amountOfKeep)
                    + scale8_LEAVING_R1_DIRTY( overlay.green,  amountOfOverlay);
    existing.blue  = scale8_LEAVING_R1_DIRTY( existing.blue,  amountOfKeep)
                    + scale8_LEAVING_R1_DIRTY( overlay.blue,   amountOfOverlay);

    cleanup_R1();
#else
    // Corrected blend method, with no loss-of-precision rounding errors
    existing.red = blend8(existing.red, overlay.red, amountOfOverlay);
    existing.green = blend8(existing.green, overlay.green, amountOfOverlay);
    existing.blue = blend8(existing.blue, overlay.blue, amountOfOverlay);
#endif

    return existing;
}

void nblend(CRGB *existing, const CRGB *overlay, uint16_t count,
            fract8 amountOfOverlay) {
    for (uint16_t i = count; i; --i) {
        nblend(*existing, *overlay, amountOfOverlay);
        ++existing;
        ++overlay;
    }
}

CRGB blend(const CRGB &p1, const CRGB &p2, fract8 amountOfP2) {
    CRGB nu(p1);
    nblend(nu, p2, amountOfP2);
    return nu;
}

CRGB *blend(const CRGB *src1, const CRGB *src2, CRGB *dest, uint16_t count,
            fract8 amountOfsrc2) {
    for (uint16_t i = 0; i < count; ++i) {
        dest[i] = blend(src1[i], src2[i], amountOfsrc2);
    }
    return dest;
}

CHSV &nblend(CHSV &existing, const CHSV &overlay, fract8 amountOfOverlay,
             TGradientDirectionCode directionCode) {
    if (amountOfOverlay == 0) {
        return existing;
    }

    if (amountOfOverlay == 255) {
        existing = overlay;
        return existing;
    }

    fract8 amountOfKeep = 255 - amountOfOverlay;

    uint8_t huedelta8 = overlay.hue - existing.hue;

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
        existing.hue = existing.hue + scale8(huedelta8, amountOfOverlay);
    } else /* directionCode == BACKWARD_HUES */
    {
        huedelta8 = -huedelta8;
        existing.hue = existing.hue - scale8(huedelta8, amountOfOverlay);
    }

    existing.sat = scale8_LEAVING_R1_DIRTY(existing.sat, amountOfKeep) +
                   scale8_LEAVING_R1_DIRTY(overlay.sat, amountOfOverlay);
    existing.val = scale8_LEAVING_R1_DIRTY(existing.val, amountOfKeep) +
                   scale8_LEAVING_R1_DIRTY(overlay.val, amountOfOverlay);

    cleanup_R1();

    return existing;
}

void nblend(CHSV *existing, const CHSV *overlay, uint16_t count,
            fract8 amountOfOverlay, TGradientDirectionCode directionCode) {
    if (existing == overlay)
        return;
    for (uint16_t i = count; i; --i) {
        nblend(*existing, *overlay, amountOfOverlay, directionCode);
        ++existing;
        ++overlay;
    }
}

CHSV blend(const CHSV &p1, const CHSV &p2, fract8 amountOfP2,
           TGradientDirectionCode directionCode) {
    CHSV nu(p1);
    nblend(nu, p2, amountOfP2, directionCode);
    return nu;
}

CHSV *blend(const CHSV *src1, const CHSV *src2, CHSV *dest, uint16_t count,
            fract8 amountOfsrc2, TGradientDirectionCode directionCode) {
    for (uint16_t i = 0; i < count; ++i) {
        dest[i] = blend(src1[i], src2[i], amountOfsrc2, directionCode);
    }
    return dest;
}

void nscale8_video(CRGB *leds, uint16_t num_leds, uint8_t scale) {
    for (uint16_t i = 0; i < num_leds; ++i) {
        leds[i].nscale8_video(scale);
    }
}

void fade_video(CRGB *leds, uint16_t num_leds, uint8_t fadeBy) {
    nscale8_video(leds, num_leds, 255 - fadeBy);
}

void fadeLightBy(CRGB *leds, uint16_t num_leds, uint8_t fadeBy) {
    nscale8_video(leds, num_leds, 255 - fadeBy);
}

void fadeToBlackBy(CRGB *leds, uint16_t num_leds, uint8_t fadeBy) {
    nscale8(leds, num_leds, 255 - fadeBy);
}

void fade_raw(CRGB *leds, uint16_t num_leds, uint8_t fadeBy) {
    nscale8(leds, num_leds, 255 - fadeBy);
}

void nscale8(CRGB *leds, uint16_t num_leds, uint8_t scale) {
    for (uint16_t i = 0; i < num_leds; ++i) {
        leds[i].nscale8(scale);
    }
}

void fadeUsingColor(CRGB *leds, uint16_t numLeds, const CRGB &colormask) {
    uint8_t fr, fg, fb;
    fr = colormask.r;
    fg = colormask.g;
    fb = colormask.b;

    for (uint16_t i = 0; i < numLeds; ++i) {
        leds[i].r = scale8_LEAVING_R1_DIRTY(leds[i].r, fr);
        leds[i].g = scale8_LEAVING_R1_DIRTY(leds[i].g, fg);
        leds[i].b = scale8(leds[i].b, fb);
    }
}

// CRGB HeatColor( uint8_t temperature)
//
// Approximates a 'black body radiation' spectrum for
// a given 'heat' level.  This is useful for animations of 'fire'.
// Heat is specified as an arbitrary scale from 0 (cool) to 255 (hot).
// This is NOT a chromatically correct 'black body radiation'
// spectrum, but it's surprisingly close, and it's fast and small.
//
// On AVR/Arduino, this typically takes around 70 bytes of program memory,
// versus 768 bytes for a full 256-entry RGB lookup table.

CRGB HeatColor(uint8_t temperature) {
    CRGB heatcolor;

    // Scale 'heat' down from 0-255 to 0-191,
    // which can then be easily divided into three
    // equal 'thirds' of 64 units each.
    uint8_t t192 = scale8_video(temperature, 191);

    // calculate a value that ramps up from
    // zero to 255 in each 'third' of the scale.
    uint8_t heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2;                 // scale up to 0..252

    // now figure out which third of the spectrum we're in:
    if (t192 & 0x80) {
        // we're in the hottest third
        heatcolor.r = 255;      // full red
        heatcolor.g = 255;      // full green
        heatcolor.b = heatramp; // ramp up blue

    } else if (t192 & 0x40) {
        // we're in the middle third
        heatcolor.r = 255;      // full red
        heatcolor.g = heatramp; // ramp up green
        heatcolor.b = 0;        // no blue

    } else {
        // we're in the coolest third
        heatcolor.r = heatramp; // ramp up red
        heatcolor.g = 0;        // no green
        heatcolor.b = 0;        // no blue
    }

    return heatcolor;
}

/// Helper function to divide a number by 16, aka four logical shift right
/// (LSR)'s. On avr-gcc, "u8 >> 4" generates a loop, which is big, and slow.
/// merely forcing it to be four /=2's causes avr-gcc to emit
/// a SWAP instruction followed by an AND 0x0F, which is faster, and smaller.
inline uint8_t lsrX4(uint8_t dividend) __attribute__((always_inline));
inline uint8_t lsrX4(uint8_t dividend) {
#if defined(__AVR__)
    dividend /= 2;
    dividend /= 2;
    dividend /= 2;
    dividend /= 2;
#else
    dividend >>= 4;
#endif
    return dividend;
}

CRGB ColorFromPaletteExtended(const CRGBPalette32 &pal, uint16_t index,
                              uint8_t brightness, TBlendType blendType) {
    // Extract the five most significant bits of the index as a palette index.
    uint8_t index_5bit = (index >> 11);
    // Calculate the 8-bit offset from the palette index.
    uint8_t offset = (uint8_t)(index >> 3);
    // Get the palette entry from the 5-bit index
    const CRGB *entry = &(pal[0]) + index_5bit;
    uint8_t red1 = entry->red;
    uint8_t green1 = entry->green;
    uint8_t blue1 = entry->blue;

    uint8_t blend = offset && (blendType != NOBLEND);
    if (blend) {
        if (index_5bit == 31) {
            entry = &(pal[0]);
        } else {
            entry++;
        }

        // Calculate the scaling factor and scaled values for the lower palette
        // value.
        uint8_t f1 = 255 - offset;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);

        // Calculate the scaled values for the neighbouring palette value.
        uint8_t red2 = entry->red;
        uint8_t green2 = entry->green;
        uint8_t blue2 = entry->blue;
        red2 = scale8_LEAVING_R1_DIRTY(red2, offset);
        green2 = scale8_LEAVING_R1_DIRTY(green2, offset);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, offset);
        cleanup_R1();

        // These sums can't overflow, so no qadd8 needed.
        red1 += red2;
        green1 += green2;
        blue1 += blue2;
    }
    if (brightness != 255) {
        nscale8x3_video(red1, green1, blue1, brightness);
    }
    return CRGB(red1, green1, blue1);
}

CRGB ColorFromPalette(const CRGBPalette16 &pal, uint8_t index,
                      uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP) {
        index = map8(index, 0, 239); // Blend range is affected by lo4 blend of
                                     // values, remap to avoid wrapping
    }

    //      hi4 = index >> 4;
    uint8_t hi4 = lsrX4(index);
    uint8_t lo4 = index & 0x0F;

    // const CRGB* entry = &(pal[0]) + hi4;
    // since hi4 is always 0..15, hi4 * sizeof(CRGB) can be a single-byte value,
    // instead of the two byte 'int' that avr-gcc defaults to.
    // So, we multiply hi4 X sizeof(CRGB), giving hi4XsizeofCRGB;
    uint8_t hi4XsizeofCRGB = hi4 * sizeof(CRGB);
    // We then add that to a base array pointer.
    const CRGB *entry = (CRGB *)((uint8_t *)(&(pal[0])) + hi4XsizeofCRGB);

    uint8_t blend = lo4 && (blendType != NOBLEND);

    uint8_t red1 = entry->red;
    uint8_t green1 = entry->green;
    uint8_t blue1 = entry->blue;

    if (blend) {

        if (hi4 == 15) {
            entry = &(pal[0]);
        } else {
            ++entry;
        }

        uint8_t f2 = lo4 << 4;
        uint8_t f1 = 255 - f2;

        //    rgb1.nscale8(f1);
        uint8_t red2 = entry->red;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        red2 = scale8_LEAVING_R1_DIRTY(red2, f2);
        red1 += red2;

        uint8_t green2 = entry->green;
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        green2 = scale8_LEAVING_R1_DIRTY(green2, f2);
        green1 += green2;

        uint8_t blue2 = entry->blue;
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, f2);
        blue1 += blue2;

        cleanup_R1();
    }

    if (brightness != 255) {
        if (brightness) {
            ++brightness; // adjust for rounding
            // Now, since brightness is nonzero, we don't need the full
            // scale8_video logic; we can just to scale8 and then add one
            // (unless scale8 fixed) to all nonzero inputs.
            if (red1) {
                red1 = scale8_LEAVING_R1_DIRTY(red1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++red1;
#endif
            }
            if (green1) {
                green1 = scale8_LEAVING_R1_DIRTY(green1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++green1;
#endif
            }
            if (blue1) {
                blue1 = scale8_LEAVING_R1_DIRTY(blue1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++blue1;
#endif
            }
            cleanup_R1();
        } else {
            red1 = 0;
            green1 = 0;
            blue1 = 0;
        }
    }

    return CRGB(red1, green1, blue1);
}

CRGB ColorFromPaletteExtended(const CRGBPalette16 &pal, uint16_t index,
                              uint8_t brightness, TBlendType blendType) {
    // Extract the four most significant bits of the index as a palette index.
    uint8_t index_4bit = index >> 12;
    // Calculate the 8-bit offset from the palette index.
    uint8_t offset = (uint8_t)(index >> 4);
    // Get the palette entry from the 4-bit index
    const CRGB *entry = &(pal[0]) + index_4bit;
    uint8_t red1 = entry->red;
    uint8_t green1 = entry->green;
    uint8_t blue1 = entry->blue;

    uint8_t blend = offset && (blendType != NOBLEND);
    if (blend) {
        if (index_4bit == 15) {
            entry = &(pal[0]);
        } else {
            entry++;
        }

        // Calculate the scaling factor and scaled values for the lower palette
        // value.
        uint8_t f1 = 255 - offset;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);

        // Calculate the scaled values for the neighbouring palette value.
        uint8_t red2 = entry->red;
        uint8_t green2 = entry->green;
        uint8_t blue2 = entry->blue;
        red2 = scale8_LEAVING_R1_DIRTY(red2, offset);
        green2 = scale8_LEAVING_R1_DIRTY(green2, offset);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, offset);
        cleanup_R1();

        // These sums can't overflow, so no qadd8 needed.
        red1 += red2;
        green1 += green2;
        blue1 += blue2;
    }
    if (brightness != 255) {
        // nscale8x3_video(red1, green1, blue1, brightness);
        nscale8x3(red1, green1, blue1, brightness);
    }
    return CRGB(red1, green1, blue1);
}

CRGB ColorFromPalette(const TProgmemRGBPalette16 &pal, uint8_t index,
                      uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP) {
        index = map8(index, 0, 239); // Blend range is affected by lo4 blend of
                                     // values, remap to avoid wrapping
    }

    //      hi4 = index >> 4;
    uint8_t hi4 = lsrX4(index);
    uint8_t lo4 = index & 0x0F;

    CRGB entry(FL_PGM_READ_DWORD_NEAR(&(pal[0]) + hi4));

    uint8_t red1 = entry.red;
    uint8_t green1 = entry.green;
    uint8_t blue1 = entry.blue;

    uint8_t blend = lo4 && (blendType != NOBLEND);

    if (blend) {

        if (hi4 == 15) {
            entry = FL_PGM_READ_DWORD_NEAR(&(pal[0]));
        } else {
            entry = FL_PGM_READ_DWORD_NEAR(&(pal[1]) + hi4);
        }

        uint8_t f2 = lo4 << 4;
        uint8_t f1 = 255 - f2;

        uint8_t red2 = entry.red;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        red2 = scale8_LEAVING_R1_DIRTY(red2, f2);
        red1 += red2;

        uint8_t green2 = entry.green;
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        green2 = scale8_LEAVING_R1_DIRTY(green2, f2);
        green1 += green2;

        uint8_t blue2 = entry.blue;
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, f2);
        blue1 += blue2;

        cleanup_R1();
    }

    if (brightness != 255) {
        if (brightness) {
            ++brightness; // adjust for rounding
            // Now, since brightness is nonzero, we don't need the full
            // scale8_video logic; we can just to scale8 and then add one
            // (unless scale8 fixed) to all nonzero inputs.
            if (red1) {
                red1 = scale8_LEAVING_R1_DIRTY(red1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++red1;
#endif
            }
            if (green1) {
                green1 = scale8_LEAVING_R1_DIRTY(green1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++green1;
#endif
            }
            if (blue1) {
                blue1 = scale8_LEAVING_R1_DIRTY(blue1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++blue1;
#endif
            }
            cleanup_R1();
        } else {
            red1 = 0;
            green1 = 0;
            blue1 = 0;
        }
    }

    return CRGB(red1, green1, blue1);
}

CRGB ColorFromPalette(const CRGBPalette32 &pal, uint8_t index,
                      uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP) {
        index = map8(index, 0, 247); // Blend range is affected by lo3 blend of
                                     // values, remap to avoid wrapping
    }

    uint8_t hi5 = index;
#if defined(__AVR__)
    hi5 /= 2;
    hi5 /= 2;
    hi5 /= 2;
#else
    hi5 >>= 3;
#endif
    uint8_t lo3 = index & 0x07;

    // const CRGB* entry = &(pal[0]) + hi5;
    // since hi5 is always 0..31, hi4 * sizeof(CRGB) can be a single-byte value,
    // instead of the two byte 'int' that avr-gcc defaults to.
    // So, we multiply hi5 X sizeof(CRGB), giving hi5XsizeofCRGB;
    uint8_t hi5XsizeofCRGB = hi5 * sizeof(CRGB);
    // We then add that to a base array pointer.
    const CRGB *entry = (CRGB *)((uint8_t *)(&(pal[0])) + hi5XsizeofCRGB);

    uint8_t red1 = entry->red;
    uint8_t green1 = entry->green;
    uint8_t blue1 = entry->blue;

    uint8_t blend = lo3 && (blendType != NOBLEND);

    if (blend) {

        if (hi5 == 31) {
            entry = &(pal[0]);
        } else {
            ++entry;
        }

        uint8_t f2 = lo3 << 5;
        uint8_t f1 = 255 - f2;

        uint8_t red2 = entry->red;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        red2 = scale8_LEAVING_R1_DIRTY(red2, f2);
        red1 += red2;

        uint8_t green2 = entry->green;
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        green2 = scale8_LEAVING_R1_DIRTY(green2, f2);
        green1 += green2;

        uint8_t blue2 = entry->blue;
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, f2);
        blue1 += blue2;

        cleanup_R1();
    }

    if (brightness != 255) {
        if (brightness) {
            ++brightness; // adjust for rounding
            // Now, since brightness is nonzero, we don't need the full
            // scale8_video logic; we can just to scale8 and then add one
            // (unless scale8 fixed) to all nonzero inputs.
            if (red1) {
                red1 = scale8_LEAVING_R1_DIRTY(red1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++red1;
#endif
            }
            if (green1) {
                green1 = scale8_LEAVING_R1_DIRTY(green1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++green1;
#endif
            }
            if (blue1) {
                blue1 = scale8_LEAVING_R1_DIRTY(blue1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++blue1;
#endif
            }
            cleanup_R1();
        } else {
            red1 = 0;
            green1 = 0;
            blue1 = 0;
        }
    }

    return CRGB(red1, green1, blue1);
}

CRGB ColorFromPalette(const TProgmemRGBPalette32 &pal, uint8_t index,
                      uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP) {
        index = map8(index, 0, 247); // Blend range is affected by lo3 blend of
                                     // values, remap to avoid wrapping
    }

    uint8_t hi5 = index;
#if defined(__AVR__)
    hi5 /= 2;
    hi5 /= 2;
    hi5 /= 2;
#else
    hi5 >>= 3;
#endif
    uint8_t lo3 = index & 0x07;

    CRGB entry(FL_PGM_READ_DWORD_NEAR(&(pal[0]) + hi5));

    uint8_t red1 = entry.red;
    uint8_t green1 = entry.green;
    uint8_t blue1 = entry.blue;

    uint8_t blend = lo3 && (blendType != NOBLEND);

    if (blend) {

        if (hi5 == 31) {
            entry = FL_PGM_READ_DWORD_NEAR(&(pal[0]));
        } else {
            entry = FL_PGM_READ_DWORD_NEAR(&(pal[1]) + hi5);
        }

        uint8_t f2 = lo3 << 5;
        uint8_t f1 = 255 - f2;

        uint8_t red2 = entry.red;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        red2 = scale8_LEAVING_R1_DIRTY(red2, f2);
        red1 += red2;

        uint8_t green2 = entry.green;
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        green2 = scale8_LEAVING_R1_DIRTY(green2, f2);
        green1 += green2;

        uint8_t blue2 = entry.blue;
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, f2);
        blue1 += blue2;

        cleanup_R1();
    }

    if (brightness != 255) {
        if (brightness) {
            ++brightness; // adjust for rounding
            // Now, since brightness is nonzero, we don't need the full
            // scale8_video logic; we can just to scale8 and then add one
            // (unless scale8 fixed) to all nonzero inputs.
            if (red1) {
                red1 = scale8_LEAVING_R1_DIRTY(red1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++red1;
#endif
            }
            if (green1) {
                green1 = scale8_LEAVING_R1_DIRTY(green1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++green1;
#endif
            }
            if (blue1) {
                blue1 = scale8_LEAVING_R1_DIRTY(blue1, brightness);
#if !(FASTLED_SCALE8_FIXED == 1)
                ++blue1;
#endif
            }
            cleanup_R1();
        } else {
            red1 = 0;
            green1 = 0;
            blue1 = 0;
        }
    }

    return CRGB(red1, green1, blue1);
}

CRGB ColorFromPalette(const CRGBPalette256 &pal, uint8_t index,
                      uint8_t brightness, TBlendType) {
    const CRGB *entry = &(pal[0]) + index;

    uint8_t red = entry->red;
    uint8_t green = entry->green;
    uint8_t blue = entry->blue;

    if (brightness != 255) {
        ++brightness; // adjust for rounding
        red = scale8_video_LEAVING_R1_DIRTY(red, brightness);
        green = scale8_video_LEAVING_R1_DIRTY(green, brightness);
        blue = scale8_video_LEAVING_R1_DIRTY(blue, brightness);
        cleanup_R1();
    }

    return CRGB(red, green, blue);
}

CRGB ColorFromPaletteExtended(const CRGBPalette256 &pal, uint16_t index,
                              uint8_t brightness, TBlendType blendType) {
    // Extract the eight most significant bits of the index as a palette index.
    uint8_t index_8bit = index >> 8;
    // Calculate the 8-bit offset from the palette index.
    uint8_t offset = index & 0xff;
    // Get the palette entry from the 8-bit index
    const CRGB *entry = &(pal[0]) + index_8bit;
    uint8_t red1 = entry->red;
    uint8_t green1 = entry->green;
    uint8_t blue1 = entry->blue;

    uint8_t blend = offset && (blendType != NOBLEND);
    if (blend) {
        if (index_8bit == 255) {
            entry = &(pal[0]);
        } else {
            entry++;
        }

        // Calculate the scaling factor and scaled values for the lower palette
        // value.
        uint8_t f1 = 255 - offset;
        red1 = scale8_LEAVING_R1_DIRTY(red1, f1);
        green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
        blue1 = scale8_LEAVING_R1_DIRTY(blue1, f1);

        // Calculate the scaled values for the neighbouring palette value.
        uint8_t red2 = entry->red;
        uint8_t green2 = entry->green;
        uint8_t blue2 = entry->blue;
        red2 = scale8_LEAVING_R1_DIRTY(red2, offset);
        green2 = scale8_LEAVING_R1_DIRTY(green2, offset);
        blue2 = scale8_LEAVING_R1_DIRTY(blue2, offset);
        cleanup_R1();

        // These sums can't overflow, so no qadd8 needed.
        red1 += red2;
        green1 += green2;
        blue1 += blue2;
    }
    if (brightness != 255) {
        // nscale8x3_video(red1, green1, blue1, brightness);
        nscale8x3(red1, green1, blue1, brightness);
    }
    return CRGB(red1, green1, blue1);
}

CHSV ColorFromPalette(const CHSVPalette16 &pal, uint8_t index,
                      uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP) {
        index = map8(index, 0, 239); // Blend range is affected by lo4 blend of
                                     // values, remap to avoid wrapping
    }

    //      hi4 = index >> 4;
    uint8_t hi4 = lsrX4(index);
    uint8_t lo4 = index & 0x0F;

    //  CRGB rgb1 = pal[ hi4];
    const CHSV *entry = &(pal[0]) + hi4;

    uint8_t hue1 = entry->hue;
    uint8_t sat1 = entry->sat;
    uint8_t val1 = entry->val;

    uint8_t blend = lo4 && (blendType != NOBLEND);

    if (blend) {

        if (hi4 == 15) {
            entry = &(pal[0]);
        } else {
            ++entry;
        }

        uint8_t f2 = lo4 << 4;
        uint8_t f1 = 255 - f2;

        uint8_t hue2 = entry->hue;
        uint8_t sat2 = entry->sat;
        uint8_t val2 = entry->val;

        // Now some special casing for blending to or from
        // either black or white.  Black and white don't have
        // proper 'hue' of their own, so when ramping from
        // something else to/from black/white, we set the 'hue'
        // of the black/white color to be the same as the hue
        // of the other color, so that you get the expected
        // brightness or saturation ramp, with hue staying
        // constant:

        // If we are starting from white (sat=0)
        // or black (val=0), adopt the target hue.
        if (sat1 == 0 || val1 == 0) {
            hue1 = hue2;
        }

        // If we are ending at white (sat=0)
        // or black (val=0), adopt the starting hue.
        if (sat2 == 0 || val2 == 0) {
            hue2 = hue1;
        }

        sat1 = scale8_LEAVING_R1_DIRTY(sat1, f1);
        val1 = scale8_LEAVING_R1_DIRTY(val1, f1);

        sat2 = scale8_LEAVING_R1_DIRTY(sat2, f2);
        val2 = scale8_LEAVING_R1_DIRTY(val2, f2);

        //    cleanup_R1();

        // These sums can't overflow, so no qadd8 needed.
        sat1 += sat2;
        val1 += val2;

        uint8_t deltaHue = (uint8_t)(hue2 - hue1);
        if (deltaHue & 0x80) {
            // go backwards
            hue1 -= scale8(256 - deltaHue, f2);
        } else {
            // go forwards
            hue1 += scale8(deltaHue, f2);
        }

        cleanup_R1();
    }

    if (brightness != 255) {
        val1 = scale8_video(val1, brightness);
    }

    return CHSV(hue1, sat1, val1);
}

CHSV ColorFromPalette(const CHSVPalette32 &pal, uint8_t index,
                      uint8_t brightness, TBlendType blendType) {
    if (blendType == LINEARBLEND_NOWRAP) {
        index = map8(index, 0, 247); // Blend range is affected by lo3 blend of
                                     // values, remap to avoid wrapping
    }

    uint8_t hi5 = index;
#if defined(__AVR__)
    hi5 /= 2;
    hi5 /= 2;
    hi5 /= 2;
#else
    hi5 >>= 3;
#endif
    uint8_t lo3 = index & 0x07;

    uint8_t hi5XsizeofCHSV = hi5 * sizeof(CHSV);
    const CHSV *entry = (CHSV *)((uint8_t *)(&(pal[0])) + hi5XsizeofCHSV);

    uint8_t hue1 = entry->hue;
    uint8_t sat1 = entry->sat;
    uint8_t val1 = entry->val;

    uint8_t blend = lo3 && (blendType != NOBLEND);

    if (blend) {

        if (hi5 == 31) {
            entry = &(pal[0]);
        } else {
            ++entry;
        }

        uint8_t f2 = lo3 << 5;
        uint8_t f1 = 255 - f2;

        uint8_t hue2 = entry->hue;
        uint8_t sat2 = entry->sat;
        uint8_t val2 = entry->val;

        // Now some special casing for blending to or from
        // either black or white.  Black and white don't have
        // proper 'hue' of their own, so when ramping from
        // something else to/from black/white, we set the 'hue'
        // of the black/white color to be the same as the hue
        // of the other color, so that you get the expected
        // brightness or saturation ramp, with hue staying
        // constant:

        // If we are starting from white (sat=0)
        // or black (val=0), adopt the target hue.
        if (sat1 == 0 || val1 == 0) {
            hue1 = hue2;
        }

        // If we are ending at white (sat=0)
        // or black (val=0), adopt the starting hue.
        if (sat2 == 0 || val2 == 0) {
            hue2 = hue1;
        }

        sat1 = scale8_LEAVING_R1_DIRTY(sat1, f1);
        val1 = scale8_LEAVING_R1_DIRTY(val1, f1);

        sat2 = scale8_LEAVING_R1_DIRTY(sat2, f2);
        val2 = scale8_LEAVING_R1_DIRTY(val2, f2);

        //    cleanup_R1();

        // These sums can't overflow, so no qadd8 needed.
        sat1 += sat2;
        val1 += val2;

        uint8_t deltaHue = (uint8_t)(hue2 - hue1);
        if (deltaHue & 0x80) {
            // go backwards
            hue1 -= scale8(256 - deltaHue, f2);
        } else {
            // go forwards
            hue1 += scale8(deltaHue, f2);
        }

        cleanup_R1();
    }

    if (brightness != 255) {
        val1 = scale8_video(val1, brightness);
    }

    return CHSV(hue1, sat1, val1);
}

CHSV ColorFromPalette(const CHSVPalette256 &pal, uint8_t index,
                      uint8_t brightness, TBlendType) {
    CHSV hsv = *(&(pal[0]) + index);

    if (brightness != 255) {
        hsv.value = scale8_video(hsv.value, brightness);
    }

    return hsv;
}

void UpscalePalette(const class CRGBPalette16 &srcpal16,
                    class CRGBPalette256 &destpal256) {
    for (int i = 0; i < 256; ++i) {
        destpal256[(uint8_t)(i)] = ColorFromPalette(srcpal16, i);
    }
}

void UpscalePalette(const class CHSVPalette16 &srcpal16,
                    class CHSVPalette256 &destpal256) {
    for (int i = 0; i < 256; ++i) {
        destpal256[(uint8_t)(i)] = ColorFromPalette(srcpal16, i);
    }
}

void UpscalePalette(const class CRGBPalette16 &srcpal16,
                    class CRGBPalette32 &destpal32) {
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t j = i * 2;
        destpal32[j + 0] = srcpal16[i];
        destpal32[j + 1] = srcpal16[i];
    }
}

void UpscalePalette(const class CHSVPalette16 &srcpal16,
                    class CHSVPalette32 &destpal32) {
    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t j = i * 2;
        destpal32[j + 0] = srcpal16[i];
        destpal32[j + 1] = srcpal16[i];
    }
}

void UpscalePalette(const class CRGBPalette32 &srcpal32,
                    class CRGBPalette256 &destpal256) {
    for (int i = 0; i < 256; ++i) {
        destpal256[(uint8_t)(i)] = ColorFromPalette(srcpal32, i);
    }
}

void UpscalePalette(const class CHSVPalette32 &srcpal32,
                    class CHSVPalette256 &destpal256) {
    for (int i = 0; i < 256; ++i) {
        destpal256[(uint8_t)(i)] = ColorFromPalette(srcpal32, i);
    }
}

#if 0
// replaced by PartyColors_p
void SetupPartyColors(CRGBPalette16& pal)
{
    fill_gradient( pal, 0, CHSV( HUE_PURPLE,255,255), 7, CHSV(HUE_YELLOW - 18,255,255), FORWARD_HUES);
    fill_gradient( pal, 8, CHSV( HUE_ORANGE,255,255), 15, CHSV(HUE_BLUE + 18,255,255), BACKWARD_HUES);
}
#endif

void nblendPaletteTowardPalette(CRGBPalette16 &current, CRGBPalette16 &target,
                                uint8_t maxChanges) {
    uint8_t *p1;
    uint8_t *p2;
    uint8_t changes = 0;

    p1 = (uint8_t *)current.entries;
    p2 = (uint8_t *)target.entries;

    const uint8_t totalChannels = sizeof(CRGBPalette16);
    for (uint8_t i = 0; i < totalChannels; ++i) {
        // if the values are equal, no changes are needed
        if (p1[i] == p2[i]) {
            continue;
        }

        // if the current value is less than the target, increase it by one
        if (p1[i] < p2[i]) {
            ++p1[i];
            ++changes;
        }

        // if the current value is greater than the target,
        // increase it by one (or two if it's still greater).
        if (p1[i] > p2[i]) {
            --p1[i];
            ++changes;
            if (p1[i] > p2[i]) {
                --p1[i];
            }
        }

        // if we've hit the maximum number of changes, exit
        if (changes >= maxChanges) {
            break;
        }
    }
}

uint8_t applyGamma_video(uint8_t brightness, float gamma) {
    float orig;
    float adj;
    orig = (float)(brightness) / (255.0);
    adj = pow(orig, gamma) * (255.0);
    uint8_t result = (uint8_t)(adj);
    if ((brightness > 0) && (result == 0)) {
        result = 1; // never gamma-adjust a positive number down to zero
    }
    return result;
}

CRGB applyGamma_video(const CRGB &orig, float gamma) {
    CRGB adj;
    adj.r = applyGamma_video(orig.r, gamma);
    adj.g = applyGamma_video(orig.g, gamma);
    adj.b = applyGamma_video(orig.b, gamma);
    return adj;
}

CRGB applyGamma_video(const CRGB &orig, float gammaR, float gammaG,
                      float gammaB) {
    CRGB adj;
    adj.r = applyGamma_video(orig.r, gammaR);
    adj.g = applyGamma_video(orig.g, gammaG);
    adj.b = applyGamma_video(orig.b, gammaB);
    return adj;
}

CRGB &napplyGamma_video(CRGB &rgb, float gamma) {
    rgb = applyGamma_video(rgb, gamma);
    return rgb;
}

CRGB &napplyGamma_video(CRGB &rgb, float gammaR, float gammaG, float gammaB) {
    rgb = applyGamma_video(rgb, gammaR, gammaG, gammaB);
    return rgb;
}

void napplyGamma_video(CRGB *rgbarray, uint16_t count, float gamma) {
    for (uint16_t i = 0; i < count; ++i) {
        rgbarray[i] = applyGamma_video(rgbarray[i], gamma);
    }
}

void napplyGamma_video(CRGB *rgbarray, uint16_t count, float gammaR,
                       float gammaG, float gammaB) {
    for (uint16_t i = 0; i < count; ++i) {
        rgbarray[i] = applyGamma_video(rgbarray[i], gammaR, gammaG, gammaB);
    }
}

} // namespace fl
