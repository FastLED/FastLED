#include <stdint.h>

#include "lib8tion.h"
#include "hsv2rgb.h"

// Functions to convert HSV colors to RGB colors.
//
//  The basically fall into two groups: spectra, and rainbows.
//  Spectra and rainbows are not the same thing.  Wikipedia has a good
//  illustration here
//   http://upload.wikimedia.org/wikipedia/commons/f/f6/Prism_compare_rainbow_01.png
//  from this article
//   http://en.wikipedia.org/wiki/Rainbow#Number_of_colours_in_spectrum_or_rainbow
//  that shows a 'spectrum' and a 'rainbow' side by side.  Among other
//  differences, you'll see that a 'rainbow' has much more yellow than
//  a plain spectrum.  "Classic" LED color washes are spectrum based, and
//  usually show very little yellow.
//
//  Wikipedia's page on HSV color space, with pseudocode for conversion
//  to RGB color space
//   http://en.wikipedia.org/wiki/HSL_and_HSV
//  Note that their conversion algorithm, which is (naturally) very popular
//  is in the "maximum brightness at any given hue" style, vs the "uniform
//  brightness for all hues" style.
//
//  You can't have both; either purple is the same brightness as red, e.g
//    red = #FF0000 and purple = #800080 -> same "total light" output
//  OR purple is 'as bright as it can be', e.g.
//    red = #FF0000 and purple = #FF00FF -> purple is much brighter than red.
//  The colorspace conversions here try to keep the apparent brightness
//  constant even as the hue varies.
//
//  Adafruit's "Wheel" function, discussed here
//   http://forums.adafruit.com/viewtopic.php?f=47&t=22483
//  is also of the "constant apparent brightness" variety.
//
//  TODO: provide the 'maximum brightness no matter what' variation.
//
//  See also some good, clear Arduino C code from Kasper Kamperman
//   http://www.kasperkamperman.com/blog/arduino/arduino-programming-hsb-to-rgb/
//  which in turn was was based on Windows C code from "nico80"
//   http://www.codeproject.com/Articles/9207/An-HSB-RGBA-colour-picker





void hsv2rgb_raw_C (const struct CHSV & hsv, struct CRGB & rgb);
void hsv2rgb_raw_avr(const struct CHSV & hsv, struct CRGB & rgb);

#if defined(__AVR__) && !defined( LIB8_ATTINY )
void hsv2rgb_raw(const struct CHSV & hsv, struct CRGB & rgb)
{
    hsv2rgb_raw_avr( hsv, rgb);
}
#else
void hsv2rgb_raw(const struct CHSV & hsv, struct CRGB & rgb)
{
    hsv2rgb_raw_C( hsv, rgb);
}
#endif



#define APPLY_DIMMING(X) (X)
#define HSV_SECTION_6 (0x20)
#define HSV_SECTION_3 (0x40)

void hsv2rgb_raw_C (const struct CHSV & hsv, struct CRGB & rgb)
{
    // Convert hue, saturation and brightness ( HSV/HSB ) to RGB
    // "Dimming" is used on saturation and brightness to make
    // the output more visually linear.
    
    // Apply dimming curves
    uint8_t value = APPLY_DIMMING( hsv.val);
    uint8_t saturation = hsv.sat;
    
    // The brightness floor is minimum number that all of
    // R, G, and B will be set to.
    uint8_t invsat = APPLY_DIMMING( 255 - saturation);
    uint8_t brightness_floor = (value * invsat) / 256;
    
    // The color amplitude is the maximum amount of R, G, and B
    // that will be added on top of the brightness_floor to
    // create the specific hue desired.
    uint8_t color_amplitude = value - brightness_floor;
    
    // Figure out which section of the hue wheel we're in,
    // and how far offset we are withing that section
    uint8_t section = hsv.hue / HSV_SECTION_3; // 0..2
    uint8_t offset = hsv.hue % HSV_SECTION_3;  // 0..63
    
    uint8_t rampup = offset; // 0..63
    uint8_t rampdown = (HSV_SECTION_3 - 1) - offset; // 63..0
    
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
    uint8_t rampup_amp_adj   = (rampup   * color_amplitude) / (256 / 4);
    uint8_t rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);
    
    // add brightness_floor offset to everything
    uint8_t rampup_adj_with_floor   = rampup_amp_adj   + brightness_floor;
    uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;
    
    
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



#if defined(__AVR__) && !defined( LIB8_ATTINY )
void hsv2rgb_raw_avr(const struct CHSV & hsv, struct CRGB & rgb)
{
    uint8_t hue, saturation, value;
    
    hue =        hsv.hue;
    saturation = hsv.sat;
    value =      hsv.val;
    
    // Saturation more useful the other way around
    saturation = 255 - saturation;
    uint8_t invsat = APPLY_DIMMING( saturation );
    
    // Apply dimming curves
    value = APPLY_DIMMING( value );
    
    // The brightness floor is minimum number that all of
    // R, G, and B will be set to, which is value * invsat
    uint8_t brightness_floor;
    
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
    uint8_t color_amplitude = value - brightness_floor;
    
    // Figure how far we are offset into the section of the
    // color wheel that we're in
    uint8_t offset = hsv.hue & (HSV_SECTION_3 - 1);  // 0..63
    uint8_t rampup = offset * 4; // 0..252
    
    
    // compute color-amplitude-scaled-down versions of rampup and rampdown
    uint8_t rampup_amp_adj;
    uint8_t rampdown_amp_adj;
    
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
    uint8_t rampup_adj_with_floor   = rampup_amp_adj   + brightness_floor;
    uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;
    
    
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
    hsv2.hue = scale8( hsv2.hue, 192);
    hsv2rgb_raw(hsv2, rgb);
}


// Sometimes the compiler will do clever things to reduce
// code size that result in a net slowdown, if it thinks that
// a variable is not used in a certain location.
// This macro does its best to convince the compiler that
// the variable is used in this location, to help control
// code motion and de-duplication that would result in a slowdown.
#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )


#define K255 255
#define K171 171
#define K85  85

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
    const uint8_t Y1 = 1;
    const uint8_t Y2 = 0;

    // G2: Whether to divide all greens by two.
    // Depends GREATLY on your particular LEDs
    const uint8_t G2 = 0;
    
    // Gscale: what to scale green down by.
    // Depends GREATLY on your particular LEDs
    const uint8_t Gscale = 0;

    
    uint8_t hue = hsv.hue;
    uint8_t sat = hsv.sat;
    uint8_t val = hsv.val;
        
    uint8_t offset = hue & 0x1F; // 0..31
    
    // offset8 = offset * 8
    uint8_t offset8 = offset;
    {
        offset8 <<= 1;
        asm volatile("");
        offset8 <<= 1;
        asm volatile("");
        offset8 <<= 1;
    }
    
    uint8_t third = scale8( offset8, (256 / 3));
        
    uint8_t r, g, b;
    
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
                    r = K171 + third;
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8( offset8, ((256 * 2) / 3));
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
                    uint8_t twothirds = scale8( offset8, ((256 * 2) / 3));
                    r = K171 - twothirds;
                    g = K171 + third;
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
                uint8_t twothirds = scale8( offset8, ((256 * 2) / 3));
                g = K171 - twothirds;
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
                r = K171 + third;
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

        nscale8x3_video( r, g, b, sat);

        uint8_t desat = 255 - sat;
        desat = scale8( desat, desat);
        
        uint8_t brightness_floor = desat;
        r += brightness_floor;
        g += brightness_floor;
        b += brightness_floor;
    }

    // Now scale everything down if we're at value < 255.
    if( val != 255 ) {
        
        val = scale8_video_LEAVING_R1_DIRTY( val, val);
        nscale8x3_video( r, g, b, val);
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


void hsv2rgb_raw(const struct CHSV * phsv, struct CRGB * prgb, int numLeds) {
    for(int i = 0; i < numLeds; i++) {
        hsv2rgb_raw(phsv[i], prgb[i]);
    }
}

void hsv2rgb_rainbow( const struct CHSV* phsv, struct CRGB * prgb, int numLeds) {
    for(int i = 0; i < numLeds; i++) {
        hsv2rgb_rainbow(phsv[i], prgb[i]);
    }
}

void hsv2rgb_spectrum( const struct CHSV* phsv, struct CRGB * prgb, int numLeds) {
    for(int i = 0; i < numLeds; i++) {
        hsv2rgb_spectrum(phsv[i], prgb[i]);
    }
}
