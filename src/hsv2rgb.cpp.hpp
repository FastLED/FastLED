/// @file hsv2rgb.cpp
/// Functions to convert from the HSV colorspace to the RGB colorspace

#include "platforms/is_platform.h"

/// Disables pragma messages and warnings
#define FASTLED_INTERNAL
#include "fl/stl/stdint.h"

#include "fl/fastled.h"
#include "fl/math_macros.h"

#include "hsv2rgb.h"


/// HSV to RGB implementation in raw C, platform independent
void hsv2rgb_raw_C (const CHSV & hsv, CRGB & rgb);
/// HSV to RGB implementation in raw C, for the AVR platform only
void hsv2rgb_raw_avr(const CHSV & hsv, CRGB & rgb);

#if defined(FL_IS_AVR) && !defined(FL_IS_AVR_ATTINY)
void hsv2rgb_raw(const CHSV & hsv, CRGB & rgb)
{
    hsv2rgb_raw_avr( hsv, rgb);
}
#else
void hsv2rgb_raw(const CHSV & hsv, CRGB & rgb)
{
    hsv2rgb_raw_C( hsv, rgb);
}
#endif


/// Apply dimming compensation to values
#define APPLY_DIMMING(X) (X)

/// Divide the color wheel into eight sections, 32 elements each
/// @todo Unused. Remove?
#define HSV_SECTION_6 (0x20)

/// Divide the color wheel into four sections, 64 elements each
/// @todo I believe this is mis-named, and should be HSV_SECTION_4
#define HSV_SECTION_3 (0x40)

/// Inline version of hsv2rgb_spectrum which returns a CRGB object.
CRGB hsv2rgb_spectrum( const CHSV& hsv) {
    CRGB rgb;
    hsv2rgb_spectrum(hsv, rgb);
    return rgb;
}

void hsv2rgb_raw_C (const CHSV & hsv, CRGB & rgb)
{
    // Convert hue, saturation and brightness ( HSV/HSB ) to RGB
    // "Dimming" is used on saturation and brightness to make
    // the output more visually linear.

    // Apply dimming curves
    fl::u8 value = APPLY_DIMMING( hsv.val);  // cppcheck-suppress selfAssignment
    fl::u8 saturation = hsv.sat;

    // The brightness floor is minimum number that all of
    // R, G, and B will be set to.
    fl::u8 invsat = APPLY_DIMMING( 255 - saturation);  // cppcheck-suppress selfAssignment
    fl::u8 brightness_floor = (value * invsat) / 256;

    // The color amplitude is the maximum amount of R, G, and B
    // that will be added on top of the brightness_floor to
    // create the specific hue desired.
    fl::u8 color_amplitude = value - brightness_floor;

    // Figure out which section of the hue wheel we're in,
    // and how far offset we are withing that section
    fl::u8 section = hsv.hue / HSV_SECTION_3; // 0..2
    fl::u8 offset = hsv.hue % HSV_SECTION_3;  // 0..63

    fl::u8 rampup = offset; // 0..63
    fl::u8 rampdown = (HSV_SECTION_3 - 1) - offset; // 63..0

    // We now scale rampup and rampdown to a 0-255 range -- at least
    // in theory, but here's where architecture-specific decsions
    // come in to play:
    // To scale them up to 0-255, we'd want to multiply by 4.
    // But in the very next step, we multiply the ramps by other
    // values and then divide the resulting product by 256.
    // So which is faster?
    //   ((ramp * 4) * othervalue) / 256
    // or
    //   ((ramp    ) * othervalue) /  64
    // It depends on your processor architecture.
    // On 8-bit AVR, the "/ 256" is just a one-cycle register move,
    // but the "/ 64" might be a multicycle shift process. So on AVR
    // it's faster do multiply the ramp values by four, and then
    // divide by 256.
    // On ARM, the "/ 256" and "/ 64" are one cycle each, so it's
    // faster to NOT multiply the ramp values by four, and just to
    // divide the resulting product by 64 (instead of 256).
    // Moral of the story: trust your profiler, not your insticts.

    // Since there's an AVR assembly version elsewhere, we'll
    // assume what we're on an architecture where any number of
    // bit shifts has roughly the same cost, and we'll remove the
    // redundant math at the source level:

    //  // scale up to 255 range
    //  //rampup *= 4; // 0..252
    //  //rampdown *= 4; // 0..252

    // compute color-amplitude-scaled-down versions of rampup and rampdown
    fl::u8 rampup_amp_adj   = (rampup   * color_amplitude) / (256 / 4);
    fl::u8 rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);

    // add brightness_floor offset to everything
    fl::u8 rampup_adj_with_floor   = rampup_amp_adj   + brightness_floor;
    fl::u8 rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;


    if( section ) {
        if( section == 1) {
            // section 1: 0x40..0x7F
            rgb.r = brightness_floor;
            rgb.g = rampdown_adj_with_floor;
            rgb.b = rampup_adj_with_floor;
        } else {
            // section 2; 0x80..0xBF
            rgb.r = rampup_adj_with_floor;
            rgb.g = brightness_floor;
            rgb.b = rampdown_adj_with_floor;
        }
    } else {
        // section 0: 0x00..0x3F
        rgb.r = rampdown_adj_with_floor;
        rgb.g = rampup_adj_with_floor;
        rgb.b = brightness_floor;
    }
}



#if defined(FL_IS_AVR) && !defined(FL_IS_AVR_ATTINY)
void hsv2rgb_raw_avr(const CHSV & hsv, CRGB & rgb)
{
    fl::u8 hue, saturation, value;

    hue =        hsv.hue;
    saturation = hsv.sat;
    value =      hsv.val;

    // Saturation more useful the other way around
    saturation = 255 - saturation;
    fl::u8 invsat = APPLY_DIMMING( saturation );  // cppcheck-suppress selfAssignment

    // Apply dimming curves
    value = APPLY_DIMMING( value );  // cppcheck-suppress selfAssignment

    // The brightness floor is minimum number that all of
    // R, G, and B will be set to, which is value * invsat
    fl::u8 brightness_floor;

    asm volatile(
                 "mul %[value], %[invsat]            \n"
                 "mov %[brightness_floor], r1        \n"
                 : [brightness_floor] "=r" (brightness_floor)
                 : [value] "r" (value),
                 [invsat] "r" (invsat)
                 : "r0", "r1"
                 );

    // The color amplitude is the maximum amount of R, G, and B
    // that will be added on top of the brightness_floor to
    // create the specific hue desired.
    fl::u8 color_amplitude = value - brightness_floor;

    // Figure how far we are offset into the section of the
    // color wheel that we're in
    fl::u8 offset = hsv.hue & (HSV_SECTION_3 - 1);  // 0..63
    fl::u8 rampup = offset * 4; // 0..252


    // compute color-amplitude-scaled-down versions of rampup and rampdown
    fl::u8 rampup_amp_adj;
    fl::u8 rampdown_amp_adj;

    asm volatile(
                 "mul %[rampup], %[color_amplitude]       \n"
                 "mov %[rampup_amp_adj], r1               \n"
                 "com %[rampup]                           \n"
                 "mul %[rampup], %[color_amplitude]       \n"
                 "mov %[rampdown_amp_adj], r1             \n"
                 : [rampup_amp_adj] "=&r" (rampup_amp_adj),
                 [rampdown_amp_adj] "=&r" (rampdown_amp_adj),
                 [rampup] "+r" (rampup)
                 : [color_amplitude] "r" (color_amplitude)
                 : "r0", "r1"
                 );


    // add brightness_floor offset to everything
    fl::u8 rampup_adj_with_floor   = rampup_amp_adj   + brightness_floor;
    fl::u8 rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;


    // keep gcc from using "X" as the index register for storing
    // results back in the return structure.  AVR's X register can't
    // do "std X+q, rnn", but the Y and Z registers can.
    // if the pointer to 'rgb' is in X, gcc will add all kinds of crazy
    // extra instructions.  Simply killing X here seems to help it
    // try Y or Z first.
    asm volatile(  ""  :  :  : "r26", "r27" );


    if( hue & 0x80 ) {
        // section 2: 0x80..0xBF
        rgb.r = rampup_adj_with_floor;
        rgb.g = brightness_floor;
        rgb.b = rampdown_adj_with_floor;
    } else {
        if( hue & 0x40) {
            // section 1: 0x40..0x7F
            rgb.r = brightness_floor;
            rgb.g = rampdown_adj_with_floor;
            rgb.b = rampup_adj_with_floor;
        } else {
            // section 0: 0x00..0x3F
            rgb.r = rampdown_adj_with_floor;
            rgb.g = rampup_adj_with_floor;
            rgb.b = brightness_floor;
        }
    }

    cleanup_R1();
}
// End of AVR asm implementation

#endif

void hsv2rgb_spectrum( const CHSV& hsv, CRGB& rgb)
{
    CHSV hsv2(hsv);
    hsv2.hue = scale8( hsv2.hue, 191);
    hsv2rgb_raw(hsv2, rgb);
}


/// Force a variable reference to avoid compiler over-optimization.
/// Sometimes the compiler will do clever things to reduce
/// code size that result in a net slowdown, if it thinks that
/// a variable is not used in a certain location.
/// This macro does its best to convince the compiler that
/// the variable is used in this location, to help control
/// code motion and de-duplication that would result in a slowdown.
#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )


/// @cond
#define K255 255
#define K171 171
#define K170 170
#define K85  85
/// @endcond

CRGB hsv2rgb_rainbow( const CHSV& hsv) {
    CRGB rgb;
    hsv2rgb_rainbow(hsv, rgb);
    return rgb;
}

void hsv2rgb_rainbow( const CHSV& hsv, CRGB& rgb)
{
    // Yellow has a higher inherent brightness than
    // any other color; 'pure' yellow is perceived to
    // be 93% as bright as white.  In order to make
    // yellow appear the correct relative brightness,
    // it has to be rendered brighter than all other
    // colors.
    // Level Y1 is a moderate boost, the default.
    // Level Y2 is a strong boost.
    const fl::u8 Y1 = 1;
    const fl::u8 Y2 = 0;

    // G2: Whether to divide all greens by two.
    // Depends GREATLY on your particular LEDs
    const fl::u8 G2 = 0;

    // Gscale: what to scale green down by.
    // Depends GREATLY on your particular LEDs
    const fl::u8 Gscale = 0;


    fl::u8 hue = hsv.hue;
    fl::u8 sat = hsv.sat;
    fl::u8 val = hsv.val;

    fl::u8 offset = hue & 0x1F; // 0..31

    // offset8 = offset * 8
    fl::u8 offset8 = offset;
    {
#if defined(FL_IS_AVR)
        // Left to its own devices, gcc turns "x <<= 3" into a loop
        // It's much faster and smaller to just do three single-bit shifts
        // So this business is to force that.
        offset8 <<= 1;
        asm volatile("");
        offset8 <<= 1;
        asm volatile("");
        offset8 <<= 1;
#else
        // On ARM and other non-AVR platforms, we just shift 3.
        offset8 <<= 3;
#endif
    }

    fl::u8 third = scale8( offset8, (256 / 3)); // max = 85

    fl::u8 r, g, b;

    if( ! (hue & 0x80) ) {
        // 0XX
        if( ! (hue & 0x40) ) {
            // 00X
            //section 0-1
            if( ! (hue & 0x20) ) {
                // 000
                //case 0: // R -> O
                r = K255 - third;
                g = third;
                b = 0;
                FORCE_REFERENCE(b);
            } else {
                // 001
                //case 1: // O -> Y
                if( Y1 ) {
                    r = K171;
                    g = K85 + third ;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
                if( Y2 ) {
                    r = K170 + third;
                    //uint8_t twothirds = (third << 1);
                    fl::u8 twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                    g = K85 + twothirds;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            }
        } else {
            //01X
            // section 2-3
            if( !  (hue & 0x20) ) {
                // 010
                //case 2: // Y -> G
                if( Y1 ) {
                    //uint8_t twothirds = (third << 1);
                    fl::u8 twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                    r = K171 - twothirds;
                    g = K170 + third;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
                if( Y2 ) {
                    r = K255 - offset8;
                    g = K255;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            } else {
                // 011
                // case 3: // G -> A
                r = 0;
                FORCE_REFERENCE(r);
                g = K255 - third;
                b = third;
            }
        }
    } else {
        // section 4-7
        // 1XX
        if( ! (hue & 0x40) ) {
            // 10X
            if( ! ( hue & 0x20) ) {
                // 100
                //case 4: // A -> B
                r = 0;
                FORCE_REFERENCE(r);
                //uint8_t twothirds = (third << 1);
                fl::u8 twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                g = K171 - twothirds; //K170?
                b = K85  + twothirds;

            } else {
                // 101
                //case 5: // B -> P
                r = third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K255 - third;

            }
        } else {
            if( !  (hue & 0x20)  ) {
                // 110
                //case 6: // P -- K
                r = K85 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K171 - third;

            } else {
                // 111
                //case 7: // K -> R
                r = K170 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K85 - third;

            }
        }
    }

    // This is one of the good places to scale the green down,
    // although the client can scale green down as well.
    if( G2 ) g = g >> 1;
    if( Gscale ) g = scale8_video_LEAVING_R1_DIRTY( g, Gscale);

    // Scale down colors if we're desaturated at all
    // and add the brightness_floor to r, g, and b.
    if( sat != 255 ) {
        if( sat == 0) {
            r = 255; b = 255; g = 255;
        } else {
            fl::u8 desat = 255 - sat;
            desat = scale8_video( desat, desat);

            fl::u8 satscale = 255 - desat;
            //satscale = sat; // uncomment to revert to pre-2021 saturation behavior

            //nscale8x3_video( r, g, b, sat);
#if (FASTLED_SCALE8_FIXED==1)
            r = scale8_LEAVING_R1_DIRTY( r, satscale);
            asm volatile("");  // Fixes jumping red pixel: https://github.com/FastLED/FastLED/pull/943
            g = scale8_LEAVING_R1_DIRTY( g, satscale);
            asm volatile("");
            b = scale8_LEAVING_R1_DIRTY( b, satscale);
            asm volatile("");
            cleanup_R1();
#else
            if( r ) r = scale8( r, satscale) + 1;
            if( g ) g = scale8( g, satscale) + 1;
            if( b ) b = scale8( b, satscale) + 1;
#endif
            fl::u8 brightness_floor = desat;
            r += brightness_floor;
            g += brightness_floor;
            b += brightness_floor;
        }
    }

    // Now scale everything down if we're at value < 255.
    if( val != 255 ) {

        val = scale8_video_LEAVING_R1_DIRTY( val, val);
        if( val == 0 ) {
            r=0; g=0; b=0;
        } else {
            // nscale8x3_video( r, g, b, val);
#if (FASTLED_SCALE8_FIXED==1)
            r = scale8_LEAVING_R1_DIRTY( r, val);
            asm volatile("");  // Fixes jumping red pixel: https://github.com/FastLED/FastLED/pull/943
            g = scale8_LEAVING_R1_DIRTY( g, val);
            asm volatile("");
            b = scale8_LEAVING_R1_DIRTY( b, val);
            asm volatile("");
            cleanup_R1();
#else
            if( r ) r = scale8( r, val) + 1;
            if( g ) g = scale8( g, val) + 1;
            if( b ) b = scale8( b, val) + 1;
#endif
        }
    }

    // Here we have the old AVR "missing std X+n" problem again
    // It turns out that fixing it winds up costing more than
    // not fixing it.
    // To paraphrase Dr Bronner, profile! profile! profile!
    //asm volatile(  ""  :  :  : "r26", "r27" );
    //asm volatile (" movw r30, r26 \n" : : : "r30", "r31");
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
}

void hsv2rgb_fullspectrum( const CHSV& hsv, CRGB& rgb) {
  const auto f = [](const int n, const fl::u8 h) -> unsigned int {
    constexpr int kZero = 0 << 8;
    constexpr int kOne  = 1 << 8;
    constexpr int kFour = 4 << 8;
    constexpr int kSix  = 6 << 8;

    const int k = ((n << 8) + 6*h) % kSix;
    const int k2 = kFour - k;
    return fl::fl_max(kZero, fl::fl_min(kOne, fl::fl_min(k, k2)));
  };

  const unsigned int chroma = hsv.v * hsv.s / 255;
  rgb.r = hsv.v - ((chroma * f(5, hsv.h)) >> 8);
  rgb.g = hsv.v - ((chroma * f(3, hsv.h)) >> 8);
  rgb.b = hsv.v - ((chroma * f(1, hsv.h)) >> 8);
}

/// Inline version of hsv2rgb_fullspectrum which returns a CRGB object.
CRGB hsv2rgb_fullspectrum( const CHSV& hsv) {
    CRGB rgb;
    hsv2rgb_fullspectrum(hsv, rgb);
    return rgb;
}


void hsv2rgb_raw(const CHSV * phsv, CRGB * prgb, int numLeds) {
    for(int i = 0; i < numLeds; ++i) {
        hsv2rgb_raw(phsv[i], prgb[i]);
    }
}

void hsv2rgb_rainbow( const CHSV* phsv, CRGB * prgb, int numLeds) {
    for(int i = 0; i < numLeds; ++i) {
        hsv2rgb_rainbow(phsv[i], prgb[i]);
    }
}

void hsv2rgb_spectrum( const CHSV* phsv, CRGB * prgb, int numLeds) {
    for(int i = 0; i < numLeds; ++i) {
        hsv2rgb_spectrum(phsv[i], prgb[i]);
    }
}

void hsv2rgb_fullspectrum( const CHSV* phsv, CRGB * prgb, int numLeds) {
    for (int i = 0; i < numLeds; ++i) {
        hsv2rgb_fullspectrum(phsv[i], prgb[i]);
    }
}

/// Convert a fractional input into a constant
#define FIXFRAC8(N,D) (((N)*256)/(D))

// This function is only an approximation, and it is not
// nearly as fast as the normal HSV-to-RGB conversion.
// See extended notes in the .h file.
CHSV rgb2hsv_approximate( const CRGB& rgb)
{
    fl::u8 r = rgb.r;
    fl::u8 g = rgb.g;
    fl::u8 b = rgb.b;
    fl::u8 h, s, v;

    // find desaturation
    fl::u8 desat = 255;
    if( r < desat) desat = r;
    if( g < desat) desat = g;
    if( b < desat) desat = b;

    // remove saturation from all channels
    r -= desat;
    g -= desat;
    b -= desat;

    //Serial.print("desat="); Serial.print(desat); Serial.println("");

    //uint8_t orig_desat = sqrt16( desat * 256);
    //Serial.print("orig_desat="); Serial.print(orig_desat); Serial.println("");

    // saturation is opposite of desaturation
    s = 255 - desat;
    //Serial.print("s.1="); Serial.print(s); Serial.println("");

    if( s != 255 ) {
        // undo 'dimming' of saturation
        s = 255 - fl::sqrt16( (255-s) * 256);
    }
    // without lib8tion: float ... ew ... sqrt... double ew, or rather, ew ^ 0.5
    // if( s != 255 ) s = (255 - (256.0 * sqrt( (float)(255-s) / 256.0)));
    //Serial.print("s.2="); Serial.print(s); Serial.println("");


    // at least one channel is now zero
    // if all three channels are zero, we had a
    // shade of gray.
    if( (r + g + b) == 0) {
        // we pick hue zero for no special reason
        return CHSV( 0, 0, 255 - s);
    }

    // scale all channels up to compensate for desaturation
    if( s < 255) {
        if( s == 0) s = 1;
        fl::u32 scaleup = 65535 / (s);
        r = ((fl::u32)(r) * scaleup) / 256;
        g = ((fl::u32)(g) * scaleup) / 256;
        b = ((fl::u32)(b) * scaleup) / 256;
    }
    //Serial.print("r.2="); Serial.print(r); Serial.println("");
    //Serial.print("g.2="); Serial.print(g); Serial.println("");
    //Serial.print("b.2="); Serial.print(b); Serial.println("");

    fl::u16 total = r + g + b;

    //Serial.print("total="); Serial.print(total); Serial.println("");

    // scale all channels up to compensate for low values
    if( total < 255) {
        if( total == 0) total = 1;
        fl::u32 scaleup = 65535 / (total);
        r = ((fl::u32)(r) * scaleup) / 256;
        g = ((fl::u32)(g) * scaleup) / 256;
        b = ((fl::u32)(b) * scaleup) / 256;
    }
    //Serial.print("r.3="); Serial.print(r); Serial.println("");
    //Serial.print("g.3="); Serial.print(g); Serial.println("");
    //Serial.print("b.3="); Serial.print(b); Serial.println("");

    if( total > 255 ) {
        v = 255;
    } else {
        v = fl::qadd8(desat,total);
        // undo 'dimming' of brightness
        if( v != 255) v = fl::sqrt16( v * 256);
        // without lib8tion: float ... ew ... sqrt... double ew, or rather, ew ^ 0.5
        // if( v != 255) v = (256.0 * sqrt( (float)(v) / 256.0));

    }

    //Serial.print("v="); Serial.print(v); Serial.println("");


#if 0

    //#else
    if( v != 255) {
        // this part could probably use refinement/rethinking,
        // (but it doesn't overflow & wrap anymore)
        fl::u16 s16;
        s16 = (s * 256);
        s16 /= v;
        //Serial.print("s16="); Serial.print(s16); Serial.println("");
        if( s16 < 256) {
            s = s16;
        } else {
            s = 255; // clamp to prevent overflow
        }
    }
#endif

    //Serial.print("s.3="); Serial.print(s); Serial.println("");


    // since this wasn't a pure shade of gray,
    // the interesting question is what hue is it



    // start with which channel is highest
    // (ties don't matter)
    fl::u8 highest = r;
    if( g > highest) highest = g;
    if( b > highest) highest = b;

    if( highest == r ) {
        // Red is highest.
        // Hue could be Purple/Pink-Red,Red-Orange,Orange-Yellow
        if( g == 0 ) {
            // if green is zero, we're in Purple/Pink-Red
            h = (HUE_PURPLE + HUE_PINK) / 2;
            h += fl::scale8( fl::qsub8(r, 128), FIXFRAC8(48,128));
        } else if ( (r - g) > g) {
            // if R-G > G then we're in Red-Orange
            h = HUE_RED;
            h += fl::scale8( g, FIXFRAC8(32,85));
        } else {
            // R-G < G, we're in Orange-Yellow
            h = HUE_ORANGE;
            h += fl::scale8( fl::qsub8((g - 85) + (171 - r), 4), FIXFRAC8(32,85)); //221
        }

    } else if ( highest == g) {
        // Green is highest
        // Hue could be Yellow-Green, Green-Aqua
        if( b == 0) {
            // if Blue is zero, we're in Yellow-Green
            //   G = 171..255
            //   R = 171..  0
            h = HUE_YELLOW;
            fl::u8 radj = fl::scale8( fl::qsub8(171,r),   47); //171..0 -> 0..171 -> 0..31
            fl::u8 gadj = fl::scale8( fl::qsub8(g,171),   96); //171..255 -> 0..84 -> 0..31;
            fl::u8 rgadj = radj + gadj;
            fl::u8 hueadv = rgadj / 2;
            h += hueadv;
            //h += scale8( qadd8( 4, qadd8((g - 128), (128 - r))),
            //             FIXFRAC8(32,255)); //
        } else {
            // if Blue is nonzero we're in Green-Aqua
            if( (g-b) > b) {
                h = HUE_GREEN;
                h += fl::scale8( b, FIXFRAC8(32,85));
            } else {
                h = HUE_AQUA;
                h += fl::scale8( fl::qsub8(b, 85), FIXFRAC8(8,42));
            }
        }

    } else /* highest == b */ {
        // Blue is highest
        // Hue could be Aqua/Blue-Blue, Blue-Purple, Purple-Pink
        if( r == 0) {
            // if red is zero, we're in Aqua/Blue-Blue
            h = HUE_AQUA + ((HUE_BLUE - HUE_AQUA) / 4);
            h += fl::scale8( fl::qsub8(b, 128), FIXFRAC8(24,128));
        } else if ( (b-r) > r) {
            // B-R > R, we're in Blue-Purple
            h = HUE_BLUE;
            h += fl::scale8( r, FIXFRAC8(32,85));
        } else {
            // B-R < R, we're in Purple-Pink
            h = HUE_PURPLE;
            h += fl::scale8( fl::qsub8(r, 85), FIXFRAC8(32,85));
        }
    }

    h += 1;
    return CHSV( h, s, v);
}

// Examples that need work:
//   0,192,192
//   192,64,64
//   224,32,32
//   252,0,126
//   252,252,0
//   252,252,126
