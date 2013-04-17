#include <stdint.h>

#include "lib8tion.h"
#include "hsv2rgb.h"


#define APPLY_DIMMING(X) (X)
#define HSV_SECTION_6 (0x20)
#define HSV_SECTION_3 (0x40)

void hsv2rgb_C (struct CHSV & hsv, struct CRGB & rgb)
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



#if defined(__AVR__)
void hsv2rgb_avr(struct CHSV & hsv, struct CRGB & rgb)
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




#define GREEN2 0

void rainbow2rgb( CHSV& hsv, CRGB& rgb)
{
    uint8_t hue = hsv.hue;
    uint8_t sat = hsv.sat;
    uint8_t val = hsv.val;
    
    val = scale8( val, val);
    
    uint8_t offset = hue & 0x1F; // 0..31
    uint8_t section = hue / 0x20; //0..7
    
    uint8_t third = scale8((offset * 8), (256 / 3));
    //uint8_t third = (offset * 8) / 3;
    
    uint8_t &r(rgb.r), &g(rgb.g), &b(rgb.b);
    
    if( section < 4 ) {
        if( section < 2 ) {
            //section 0-1
            if( section == 0) {
                //case 0: // R -> O
                r = 255 - third;
#if GREEN2 == 0
                g = third;
#else
                g = third / 2;
#endif
                b = 0;
                //break;
            } else {
                // ADJ Yellow high
                //case 1: // O -> Y
                r = 171 + third;
#if GREEN2 == 0
                g = (85 + (third * 2));
#else
                g = (85 / 2)  + third;
#endif
                b = 0;
                //break;
            }
        } else {
            // section 2-3
            if( section == 2) {
                // ADJ Yellow high
                //case 2: // Y -> G
                r = 255 - (offset * 8);
#if GREEN2 == 0
                g = 255;
#else
                g = 255 / 2;
#endif
                b = 0;
                //break;
            } else {
                // case 3: // G -> A
                r = 0;
#if GREEN2 == 0
                g = (255 - third);
#else
                g = (255 - third) / 2;
#endif
                b = third;
                //break;
            }
        }
    } else {
        // section 4-7
        if( section < 6) {
            if( section == 4) {
                //case 4: // A -> B
                r = 0;
#if GREEN2 == 0
                g = (171 - (third * 2));
#else
                g = (171 / 2) - third;
#endif
                b = 85 + (third * 2);
                //break;
            } else {
                //case 5: // B -> P
                r = third;
                g = 0;
                b = 255 - third;
                //break;
            }
        } else {
            if( section == 6 ) {
                //case 6: // P -- K
                r = 85 + third;
                g = 0;
                b = 171 - third;
                //break;
            } else {
                //case 7: // K -> R
                r = 171 + third;
                g = 0;
                b = 85 - third;
                //break;
            }
        }
    }
    
    nscale8x3_video( r, g, b, sat);
    
    uint8_t desat = 255 - sat;
    desat = scale8(desat, desat);
    uint8_t brightness_floor = desat;
    
    r += brightness_floor;
    g += brightness_floor;
    b += brightness_floor;
    
    nscale8x3_video( r, g, b, val);
}


#endif

