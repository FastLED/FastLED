#include <stdint.h>

#include <avr/pgmspace.h>

#include "hsv2rgb.h"
#include "colorutils.h"


void fill_solid( struct CRGB * pFirstLED, int numToFill,
                const struct CRGB& color)
{
    for( int i = 0; i < numToFill; i++) {
        pFirstLED[i] = color;
    }
}

void fill_rainbow( struct CRGB * pFirstLED, int numToFill,
                  uint8_t initialhue,
                  uint8_t deltahue )
{
    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 255;
    for( int i = 0; i < numToFill; i++) {
        hsv2rgb_rainbow( hsv, pFirstLED[i]);
        hsv.hue += deltahue;
    }
}


#define saccum87 int16_t

void fill_gradient( CRGB* leds,
                    uint16_t startpos, CHSV startcolor,
                    uint16_t endpos,   CHSV endcolor,
                    TGradientDirectionCode directionCode )
{
    // if the points are in the wrong order, straighten them
    if( endpos < startpos ) {
        uint16_t t = endpos;
        CHSV tc = endcolor;
        startpos = t;
        startcolor = tc;
        endcolor = startcolor;
        endpos = startpos;
    }
    
    saccum87 huedistance87;
    saccum87 satdistance87;
    saccum87 valdistance87;
    
    satdistance87 = (endcolor.sat - startcolor.sat) << 7;
    valdistance87 = (endcolor.val - startcolor.val) << 7;
    
    uint8_t huedelta8 = endcolor.hue - startcolor.hue;
    
    if( directionCode == SHORTEST_HUES ) {
        directionCode = FORWARD_HUES;
        if( huedelta8 > 127) {
            directionCode = BACKWARD_HUES;
        }
    }
    
    if( directionCode == LONGEST_HUES ) {
        directionCode = FORWARD_HUES;
        if( huedelta8 < 128) {
            directionCode = BACKWARD_HUES;
        }
    }
    
    if( directionCode == FORWARD_HUES) {
        huedistance87 = huedelta8 << 7;
    }
    else /* directionCode == BACKWARD_HUES */
    {
        huedistance87 = (uint8_t)(256 - huedelta8) << 7;
        huedistance87 = -huedistance87;
    }
    
    uint16_t pixeldistance = endpos - startpos;
    uint16_t p2 = pixeldistance / 2;
    int16_t divisor = p2 ? p2 : 1;
    saccum87 huedelta87 = huedistance87 / divisor;
    saccum87 satdelta87 = satdistance87 / divisor;
    saccum87 valdelta87 = valdistance87 / divisor;
    
    accum88 hue88 = startcolor.hue << 8;
    accum88 sat88 = startcolor.sat << 8;
    accum88 val88 = startcolor.val << 8;
    for( uint16_t i = startpos; i <= endpos; i++) {
        leds[i] = CHSV( hue88 >> 8, sat88 >> 8, val88 >> 8);
        hue88 += huedelta87;
        sat88 += satdelta87;
        val88 += valdelta87;
    }
}



void nscale8_video( CRGB* leds, uint16_t num_leds, uint8_t scale)
{
    for( uint16_t i = 0; i < num_leds; i++) {
        leds[i].nscale8_video( scale);
    }
}

void fade_video(CRGB* leds, uint16_t num_leds, uint8_t fadeBy)
{
    nscale8_video( leds, num_leds, 255 - fadeBy);
}

void fadeLightBy(CRGB* leds, uint16_t num_leds, uint8_t fadeBy)
{
    nscale8_video( leds, num_leds, 255 - fadeBy);
}


void fadeToBlackBy( CRGB* leds, uint16_t num_leds, uint8_t fadeBy)
{
    nscale8( leds, num_leds, 255 - fadeBy);
}

void fade_raw( CRGB* leds, uint16_t num_leds, uint8_t fadeBy)
{
    nscale8( leds, num_leds, 255 - fadeBy);
}

void nscale8_raw( CRGB* leds, uint16_t num_leds, uint8_t scale)
{
    nscale8( leds, num_leds, scale);
}

void nscale8( CRGB* leds, uint16_t num_leds, uint8_t scale)
{
    for( uint16_t i = 0; i < num_leds; i++) {
        leds[i].nscale8( scale);
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

CRGB HeatColor( uint8_t temperature)
{
    CRGB heatcolor;
    
    // Scale 'heat' down from 0-255 to 0-191,
    // which can then be easily divided into three
    // equal 'thirds' of 64 units each.
    uint8_t t192 = scale8_video( temperature, 192);
    
    // calculate a value that ramps up from
    // zero to 255 in each 'third' of the scale.
    uint8_t heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2; // scale up to 0..252
    
    // now figure out which third of the spectrum we're in:
    if( t192 & 0x80) {
        // we're in the hottest third
        heatcolor.r = 255; // full red
        heatcolor.g = 255; // full green
        heatcolor.b = heatramp; // ramp up blue
        
    } else if( t192 & 0x40 ) {
        // we're in the middle third
        heatcolor.r = 255; // full red
        heatcolor.g = heatramp; // ramp up green
        heatcolor.b = 0; // no blue
        
    } else {
        // we're in the coolest third
        heatcolor.r = heatramp; // ramp up red
        heatcolor.g = 0; // no green
        heatcolor.b = 0; // no blue
    }
    
    return heatcolor;
}



CRGB ColorFromPalette( const CRGBPalette16& pal, uint8_t index, uint8_t brightness, TInterpolationType interpolationType)
{
    uint8_t hi4 = index >> 4;
    uint8_t lo4 = index & 0x0F;
    
    //  CRGB rgb1 = pal[ hi4];
    const CRGB* entry = pal + hi4;
    uint8_t red1   = entry->red;
    uint8_t green1 = entry->green;
    uint8_t blue1  = entry->blue;
    
    uint8_t interpolate = lo4 && (interpolationType != INTERPOLATION_NONE);
    
    if( interpolate ) {
        
        if( hi4 == 15 ) {
            entry = pal;
        } else {
            entry++;
        }
        
        uint8_t f2 = lo4 << 4;
        uint8_t f1 = 256 - f2;
        
        //    rgb1.nscale8(f1);
        red1   = scale8_LEAVING_R1_DIRTY( red1,   f1);
        green1 = scale8_LEAVING_R1_DIRTY( green1, f1);
        blue1  = scale8_LEAVING_R1_DIRTY( blue1,  f1);
                
        //    cleanup_R1();
        
        //    CRGB rgb2 = pal[ hi4];
        //    rgb2.nscale8(f2);
        uint8_t red2   = entry->red;
        uint8_t green2 = entry->green;
        uint8_t blue2  = entry->blue;
        red2   = scale8_LEAVING_R1_DIRTY( red2,   f2);
        green2 = scale8_LEAVING_R1_DIRTY( green2, f2);
        blue2  = scale8_LEAVING_R1_DIRTY( blue2,  f2);
        
        cleanup_R1();
        
        // These sums can't overflow, so no qadd8 needed.
        red1   += red2;
        green1 += green2;
        blue1  += blue2;

    }
    
    if( brightness != 255) {
        nscale8x3_video( red1, green1, blue1, brightness);
    }
    
    return CRGB( red1, green1, blue1);  
}


CRGB ColorFromPalette( const CRGBPalette256& pal, uint8_t index, uint8_t brightness, TInterpolationType)
{
    const CRGB* entry = pal + index;

    uint8_t red   = entry->red;
    uint8_t green = entry->green;
    uint8_t blue  = entry->blue;
    
    if( brightness != 255) {
        nscale8x3_video( red, green, blue, brightness);
    }
    
    return CRGB( red, green, blue);
}

typedef prog_uint32_t TProgmemPalette16[16];

const TProgmemPalette16 CloudPalette_p PROGMEM =
{
    CRGB::Blue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    
    CRGB::Blue,
    CRGB::DarkBlue,
    CRGB::SkyBlue,
    CRGB::SkyBlue,
    
    CRGB::LightBlue,
    CRGB::White,
    CRGB::LightBlue,
    CRGB::SkyBlue
};

const TProgmemPalette16 LavaPalette_p PROGMEM =
{
    CRGB::Black,
    CRGB::Maroon,
    CRGB::Black,
    CRGB::Maroon,
    
    CRGB::DarkRed,
    CRGB::Maroon,
    CRGB::DarkRed,
    
    CRGB::DarkRed,
    CRGB::DarkRed,
    CRGB::Red,
    CRGB::Orange,
    
    CRGB::White,
    CRGB::Orange,
    CRGB::Red,
    CRGB::DarkRed
};


const TProgmemPalette16 OceanPalette_p PROGMEM =
{
    CRGB::MidnightBlue,
    CRGB::DarkBlue,
    CRGB::MidnightBlue,
    CRGB::Navy,
    
    CRGB::DarkBlue,
    CRGB::MediumBlue,
    CRGB::SeaGreen,
    CRGB::Teal,
    
    CRGB::CadetBlue,
    CRGB::Blue,
    CRGB::DarkCyan,
    CRGB::CornflowerBlue,
    
    CRGB::Aquamarine,
    CRGB::SeaGreen,
    CRGB::Aqua,
    CRGB::LightSkyBlue
};

const TProgmemPalette16 ForestPalette_p PROGMEM =
{
    CRGB::DarkGreen,
    CRGB::DarkGreen,
    CRGB::DarkOliveGreen,
    CRGB::DarkGreen,
    
    CRGB::Green,
    CRGB::ForestGreen,
    CRGB::OliveDrab,
    CRGB::Green,
    
    CRGB::SeaGreen,
    CRGB::MediumAquamarine,
    CRGB::LimeGreen,
    CRGB::YellowGreen,

    CRGB::LightGreen,
    CRGB::LawnGreen,
    CRGB::MediumAquamarine,
    CRGB::ForestGreen
};


void InitPalette(CRGBPalette16& pal, const TProgmemPalette16 ppp)
{
    for( uint8_t i = 0; i < 16; i++) {
        pal[i] =  pgm_read_dword_near( ppp + i);
    }
}

void UpscalePalette(const CRGBPalette16& srcpal16, CRGBPalette256& destpal256)
{
    for( int i = 0; i < 256; i++) {
        destpal256[i] = ColorFromPalette( srcpal16, i);
    }
}

void SetCloudPalette(CRGBPalette16& pal)
{
    InitPalette( pal, CloudPalette_p);
}

void SetLavaPalette(CRGBPalette16& pal)
{
    InitPalette( pal, LavaPalette_p);
}

void SetOceanPalette(CRGBPalette16& pal)
{
    InitPalette( pal, OceanPalette_p);
}

void SetForestPalette(CRGBPalette16& pal)
{
    InitPalette( pal, ForestPalette_p);
}

void SetRainbowPalette(CRGBPalette16& pal)
{
    for( uint8_t c = 0; c < 16; c += 1) {
        uint8_t hue = c << 4;
        pal[c] = CHSV(  hue, 255, 255);
    }
}

void SetRainbowStripesPalette(CRGBPalette16& pal)
{
    for( uint8_t c = 0; c < 16; c += 2) {
        uint8_t hue = c << 4;
        pal[c] = CHSV(  hue, 255, 255);
        pal[c+1] = CRGB::Black;
    }
}

void fill_palette(CRGB* L, uint16_t N, uint8_t startIndex, uint8_t incIndex,
                  const CRGBPalette16& pal, uint8_t brightness, TInterpolationType interpType)
{
    uint8_t colorIndex = startIndex;
    for( uint16_t i = 0; i < N; i++) {
        L[i] = ColorFromPalette( pal, colorIndex, brightness, interpType);
        colorIndex += incIndex;
    }
}


void fill_palette(CRGB* L, uint16_t N, uint8_t startIndex, uint8_t incIndex,
                  const CRGBPalette256& pal, uint8_t brightness, TInterpolationType interpType)
{
    uint8_t colorIndex = startIndex;
    for( uint16_t i = 0; i < N; i++) {
        L[i] = ColorFromPalette( pal, colorIndex, brightness, interpType);
        colorIndex += incIndex;
    }
}
