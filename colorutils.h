#ifndef __INC_COLORUTILS_H
#define __INC_COLORUTILS_H

#include <avr/pgmspace.h>

#include "pixeltypes.h"


// fill_solid -   fill a range of LEDs with a solid color
//                Example: fill_solid( leds, NUM_LEDS, CRGB(50,0,200));

void fill_solid( struct CRGB * leds, int numToFill,
                 const struct CRGB& color);

void fill_solid( struct CHSV* targetArray, int numToFill,
				 const struct CHSV& hsvColor);
                 

// fill_rainbow - fill a range of LEDs with a rainbow of colors, at
//                full saturation and full value (brightness)
void fill_rainbow( struct CRGB * pFirstLED, int numToFill,
                   uint8_t initialhue,
                   uint8_t deltahue = 5);
                   
void fill_rainbow( struct CHSV * targetArray, int numToFill,
                   uint8_t initialhue,
                   uint8_t deltahue = 5);


// fill_gradient - fill an array of colors with a smooth HSV gradient
//                 between two specified HSV colors.
//                 Since 'hue' is a value around a color wheel,
//                 there are always two ways to sweep from one hue
//                 to another.
//                 This function lets you specify which way you want
//                 the hue gradient to sweep around the color wheel:
//                   FORWARD_HUES: hue always goes clockwise
//                   BACKWARD_HUES: hue always goes counter-clockwise
//                   SHORTEST_HUES: hue goes whichever way is shortest
//                   LONGEST_HUES: hue goes whichever way is longest
//                 The default is SHORTEST_HUES, as this is nearly
//                 always what is wanted.
//
// fill_gradient can write the gradient colors EITHER
//     (1) into an array of CRGBs (e.g., into leds[] array, or an RGB Palette)
//   OR 
//     (2) into an array of CHSVs (e.g. an HSV Palette).
//
//   In the case of writing into a CRGB array, the gradient is 
//   computed in HSV space, and then HSV values are converted to RGB 
//   as they're written into the RGB array.

typedef enum { FORWARD_HUES, BACKWARD_HUES, SHORTEST_HUES, LONGEST_HUES } TGradientDirectionCode;



#define saccum87 int16_t

template <typename T>
void fill_gradient( T* targetArray,
                    uint16_t startpos, CHSV startcolor,
                    uint16_t endpos,   CHSV endcolor,
                    TGradientDirectionCode directionCode  = SHORTEST_HUES )
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
    
    // If we're fading toward black (val=0) or white (sat=0),
    // then set the endhue to the starthue.
    // This lets us ramp smoothly to black or white, regardless
    // of what 'hue' was set in the endcolor (since it doesn't matter)
    if( endcolor.value == 0 || endcolor.saturation == 0) {
        endcolor.hue = startcolor.hue;
    }
    
    // Similarly, if we're fading in from black (val=0) or white (sat=0)
    // then set the starthue to the endhue.
    // This lets us ramp smoothly up from black or white, regardless
    // of what 'hue' was set in the startcolor (since it doesn't matter)
    if( startcolor.value == 0 || startcolor.saturation == 0) {
        startcolor.hue = endcolor.hue;
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
    int16_t divisor = pixeldistance ? pixeldistance : 1;
    
    saccum87 huedelta87 = huedistance87 / divisor;
    saccum87 satdelta87 = satdistance87 / divisor;
    saccum87 valdelta87 = valdistance87 / divisor;
    
    huedelta87 *= 2;
    satdelta87 *= 2;
    valdelta87 *= 2;
    
    accum88 hue88 = startcolor.hue << 8;
    accum88 sat88 = startcolor.sat << 8;
    accum88 val88 = startcolor.val << 8;
    for( uint16_t i = startpos; i <= endpos; i++) {
        targetArray[i] = CHSV( hue88 >> 8, sat88 >> 8, val88 >> 8);
        hue88 += huedelta87;
        sat88 += satdelta87;
        val88 += valdelta87;
    }
}


// Convenience functions to fill an array of colors with a
// two-color, three-color, or four-color gradient
template <typename T>
void fill_gradient( T* targetArray, uint16_t numLeds, const CHSV& c1, const CHSV& c2, 
					TGradientDirectionCode directionCode = SHORTEST_HUES )
{
    uint16_t last = numLeds - 1;
    fill_gradient( targetArray, 0, c1, last, c2, directionCode);
}

template <typename T>
void fill_gradient( T* targetArray, uint16_t numLeds, 
					const CHSV& c1, const CHSV& c2, const CHSV& c3, 
					TGradientDirectionCode directionCode = SHORTEST_HUES )
{
    uint16_t half = (numLeds / 2);
    uint16_t last = numLeds - 1;
    fill_gradient( targetArray,    0, c1, half, c2, directionCode);
    fill_gradient( targetArray, half, c2, last, c3, directionCode);
}

template <typename T>
void fill_gradient( T* targetArray, uint16_t numLeds, 
					const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4, 
					TGradientDirectionCode directionCode = SHORTEST_HUES )
{
    uint16_t onethird = (numLeds / 3);
    uint16_t twothirds = ((numLeds * 2) / 3);
    uint16_t last = numLeds - 1;
    fill_gradient( targetArray,         0, c1,  onethird, c2, directionCode);
    fill_gradient( targetArray,  onethird, c2, twothirds, c3, directionCode);
    fill_gradient( targetArray, twothirds, c3,      last, c4, directionCode);
}

// convenience synonym
#define fill_gradient_HSV fill_gradient


// fill_gradient_RGB - fill a range of LEDs with a smooth RGB gradient
//                     between two specified RGB colors.
//                     Unlike HSV, there is no 'color wheel' in RGB space,
//                     and therefore there's only one 'direction' for the
//                     gradient to go, and no 'direction code' is needed.
void fill_gradient_RGB( CRGB* leds,
                       uint16_t startpos, CRGB startcolor,
                       uint16_t endpos,   CRGB endcolor );
void fill_gradient_RGB( CRGB* leds, uint16_t numLeds, const CRGB& c1, const CRGB& c2);
void fill_gradient_RGB( CRGB* leds, uint16_t numLeds, const CRGB& c1, const CRGB& c2, const CRGB& c3);
void fill_gradient_RGB( CRGB* leds, uint16_t numLeds, const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4);


// fadeLightBy and fade_video - reduce the brightness of an array
//                              of pixels all at once.  Guaranteed
//                              to never fade all the way to black.
//                              (The two names are synonyms.)
void fadeLightBy(   CRGB* leds, uint16_t num_leds, uint8_t fadeBy);
void fade_video(    CRGB* leds, uint16_t num_leds, uint8_t fadeBy);

// nscale8_video - scale down the brightness of an array of pixels
//                 all at once.  Guaranteed to never scale a pixel
//                 all the way down to black, unless 'scale' is zero.
void nscale8_video( CRGB* leds, uint16_t num_leds, uint8_t scale);

// fadeToBlackBy and fade_raw - reduce the brightness of an array
//                              of pixels all at once.  These
//                              functions will eventually fade all
//                              the way to black.
//                              (The two names are synonyms.)
void fadeToBlackBy( CRGB* leds, uint16_t num_leds, uint8_t fadeBy);
void fade_raw(      CRGB* leds, uint16_t num_leds, uint8_t fadeBy);

// nscale8 - scale down the brightness of an array of pixels
//           all at once.  This function can scale pixels all the
//           way down to black even if 'scale' is not zero.
void nscale8(       CRGB* leds, uint16_t num_leds, uint8_t scale);



// Pixel blending
//
// blend - computes a new color blended some fraction of the way
//         between two other colors.
CRGB  blend( const CRGB& p1, const CRGB& p2, fract8 amountOfP2 );

CHSV  blend( const CHSV& p1, const CHSV& p2, fract8 amountOfP2,
            TGradientDirectionCode directionCode = SHORTEST_HUES );

// blend - computes a new color blended array of colors, each
//         a given fraction of the way between corresponding
//         elements of two source arrays of colors.
//         Useful for blending palettes.
CRGB* blend( const CRGB* src1, const CRGB* src2, CRGB* dest,
             uint16_t count, fract8 amountOfsrc2 );

CHSV* blend( const CHSV* src1, const CHSV* src2, CHSV* dest,
            uint16_t count, fract8 amountOfsrc2,
            TGradientDirectionCode directionCode = SHORTEST_HUES );

// nblend - destructively modifies one color, blending
//          in a given fraction of an overlay color
CRGB& nblend( CRGB& existing, const CRGB& overlay, fract8 amountOfOverlay );

CHSV& nblend( CHSV& existing, const CHSV& overlay, fract8 amountOfOverlay,
             TGradientDirectionCode directionCode = SHORTEST_HUES );

// nblend - destructively blends a given fraction of
//          a new color array into an existing color array
void  nblend( CRGB* existing, CRGB* overlay, uint16_t count, fract8 amountOfOverlay);

void  nblend( CHSV* existing, CHSV* overlay, uint16_t count, fract8 amountOfOverlay,
             TGradientDirectionCode directionCode = SHORTEST_HUES);



// CRGB HeatColor( uint8_t temperature)
//
// Approximates a 'black body radiation' spectrum for
// a given 'heat' level.  This is useful for animations of 'fire'.
// Heat is specified as an arbitrary scale from 0 (cool) to 255 (hot).
// This is NOT a chromatically correct 'black body radiation'
// spectrum, but it's surprisingly close, and it's fast and small.
CRGB HeatColor( uint8_t temperature);


// Palettes
//
// RGB Palettes map an 8-bit value (0..255) to an RGB color.
//
// You can create any color palette you wish; a couple of starters
// are provided: Forest, Clouds, Lava, Ocean, Rainbow, and Rainbow Stripes.
//
// Palettes come in the traditional 256-entry variety, which take
// up 768 bytes of RAM, and lightweight 16-entry varieties.  The 16-entry
// variety automatically interpolates between its entries to produce
// a full 256-element color map, but at a cost of only 48 bytes or RAM.
//
// Basic operation is like this: (example shows the 16-entry variety)
// 1. Declare your palette storage:
//    CRGBPalette16 myPalette;
//
// 2. Fill myPalette with your own 16 colors, or with a preset color scheme.
//    You can specify your 16 colors a variety of ways:
//      CRGBPalette16 myPalette(
//          CRGB::Black,
//          CRGB::Black,
//          CRGB::Red,
//          CRGB::Yellow,
//          CRGB::Green,
//          CRGB::Blue,
//          CRGB::Purple,
//          CRGB::Black,
//
//          0x100000,
//          0x200000,
//          0x400000,
//          0x800000,
//
//          CHSV( 30,255,255),
//          CHSV( 50,255,255),
//          CHSV( 70,255,255),
//          CHSV( 90,255,255)
//      );
//
//    Or you can initiaize your palette with a preset color scheme:
//      myPalette = RainbowStripesColors_p;
//
// 3. Any time you want to set a pixel to a color from your palette, use
//    "ColorFromPalette(...)" as shown:
//
//      uint8_t index = /* any value 0..255 */;
//      leds[i] = ColorFromPalette( myPalette, index);
//
//    Even though your palette has only 16 explicily defined entries, you
//    can use an 'index' from 0..255.  The 16 explicit palette entries will
//    be spread evenly across the 0..255 range, and the intermedate values
//    will be RGB-interpolated between adjacent explicit entries.
//
//    It's easier to use than it sounds.
//

class CRGBPalette16;
class CRGBPalette256;
class CHSVPalette16;
class CHSVPalette256;
typedef uint32_t TProgmemRGBPalette16[16];
typedef uint32_t TProgmemHSVPalette16[16];
#define TProgmemPalette16 TProgmemRGBPalette16 

// Convert a 16-entry palette to a 256-entry palette
void UpscalePalette(const struct CRGBPalette16& srcpal16, struct CRGBPalette256& destpal256);
void UpscalePalette(const struct CHSVPalette16& srcpal16, struct CHSVPalette256& destpal256);

class CHSVPalette16 {
public:
    CHSV entries[16];
    CHSVPalette16() {};
    CHSVPalette16( const CHSV& c00,const CHSV& c01,const CHSV& c02,const CHSV& c03,
                    const CHSV& c04,const CHSV& c05,const CHSV& c06,const CHSV& c07,
                    const CHSV& c08,const CHSV& c09,const CHSV& c10,const CHSV& c11,
                    const CHSV& c12,const CHSV& c13,const CHSV& c14,const CHSV& c15 )
    {
        entries[0]=c00; entries[1]=c01; entries[2]=c02; entries[3]=c03;
        entries[4]=c04; entries[5]=c05; entries[6]=c06; entries[7]=c07;
        entries[8]=c08; entries[9]=c09; entries[10]=c10; entries[11]=c11;
        entries[12]=c12; entries[13]=c13; entries[14]=c14; entries[15]=c15;
    };
    
    CHSVPalette16( const CHSVPalette16& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
    }
    CHSVPalette16& operator=( const CHSVPalette16& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
        return *this;
    }
    
    CHSVPalette16( const TProgmemHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
            CRGB xyz   =  pgm_read_dword_near( rhs + i);
            entries[i].hue = xyz.red;
            entries[i].sat = xyz.green;
            entries[i].val = xyz.blue;
        }
    }
    CHSVPalette16& operator=( const TProgmemHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
            CRGB xyz   =  pgm_read_dword_near( rhs + i);
            entries[i].hue = xyz.red;
            entries[i].sat = xyz.green;
            entries[i].val = xyz.blue;
        }
        return *this;
    }
    
    inline CHSV& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return entries[x];
    }
    inline const CHSV& operator[] (uint8_t x) const __attribute__((always_inline))
    {
        return entries[x];
    }
    
    inline CHSV& operator[] (int x) __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    inline const CHSV& operator[] (int x) const __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    
    operator CHSV*()
    {
        return &(entries[0]);
    }
    
    CHSVPalette16( const CHSV& c1)
    {
        fill_solid( &(entries[0]), 16, c1);
    }
    CHSVPalette16( const CHSV& c1, const CHSV& c2)
    {
        fill_gradient( &(entries[0]), 16, c1, c2);
    }
    CHSVPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3);
    }
    CHSVPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3, c4);
    }
    
};

class CHSVPalette256 {
public:
    CHSV entries[256];
    CHSVPalette256() {};
    CHSVPalette256( const CHSV& c00,const CHSV& c01,const CHSV& c02,const CHSV& c03,
                  const CHSV& c04,const CHSV& c05,const CHSV& c06,const CHSV& c07,
                  const CHSV& c08,const CHSV& c09,const CHSV& c10,const CHSV& c11,
                  const CHSV& c12,const CHSV& c13,const CHSV& c14,const CHSV& c15 )
    {
        CHSVPalette16 p16(c00,c01,c02,c03,c04,c05,c06,c07,
                          c08,c09,c10,c11,c12,c13,c14,c15);
        *this = p16;
    };
    
    CHSVPalette256( const CHSVPalette256& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
    }
    CHSVPalette256& operator=( const CHSVPalette256& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
        return *this;
    }
    
    CHSVPalette256( const CHSVPalette16& rhs16)
    {
        UpscalePalette( rhs16, *this);
    }
    CHSVPalette256& operator=( const CHSVPalette16& rhs16)
    {
        UpscalePalette( rhs16, *this);
        return *this;
    }
    
    CHSVPalette256( const TProgmemRGBPalette16& rhs)
    {
        CHSVPalette16 p16(rhs);
        *this = p16;
    }
    CHSVPalette256& operator=( const TProgmemRGBPalette16& rhs)
    {
        CHSVPalette16 p16(rhs);
        *this = p16;
        return *this;
    }
    
    inline CHSV& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return entries[x];
    }
    inline const CHSV& operator[] (uint8_t x) const __attribute__((always_inline))
    {
        return entries[x];
    }
    
    inline CHSV& operator[] (int x) __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    inline const CHSV& operator[] (int x) const __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }

    operator CHSV*()
    {
        return &(entries[0]);
    }
    
    CHSVPalette256( const CHSV& c1)
    {
      fill_solid( &(entries[0]), 256, c1);
    }
    CHSVPalette256( const CHSV& c1, const CHSV& c2)
    {
        fill_gradient( &(entries[0]), 256, c1, c2);
    }
    CHSVPalette256( const CHSV& c1, const CHSV& c2, const CHSV& c3)
    {
        fill_gradient( &(entries[0]), 256, c1, c2, c3);
    }
    CHSVPalette256( const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4)
    {
        fill_gradient( &(entries[0]), 256, c1, c2, c3, c4);
    }
};

class CRGBPalette16 {
public:
    CRGB entries[16];
    CRGBPalette16() {};
    CRGBPalette16( const CRGB& c00,const CRGB& c01,const CRGB& c02,const CRGB& c03,
                    const CRGB& c04,const CRGB& c05,const CRGB& c06,const CRGB& c07,
                    const CRGB& c08,const CRGB& c09,const CRGB& c10,const CRGB& c11,
                    const CRGB& c12,const CRGB& c13,const CRGB& c14,const CRGB& c15 )
    {
        entries[0]=c00; entries[1]=c01; entries[2]=c02; entries[3]=c03;
        entries[4]=c04; entries[5]=c05; entries[6]=c06; entries[7]=c07;
        entries[8]=c08; entries[9]=c09; entries[10]=c10; entries[11]=c11;
        entries[12]=c12; entries[13]=c13; entries[14]=c14; entries[15]=c15;
    };
    
    CRGBPalette16( const CRGBPalette16& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
    }
    CRGBPalette16& operator=( const CRGBPalette16& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
        return *this;
    }
    
    CRGBPalette16( const CHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
    		entries[i] = rhs.entries[i]; // implicit HSV-to-RGB conversion
        }
    }    
    CRGBPalette16& operator=( const CHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
    		entries[i] = rhs.entries[i]; // implicit HSV-to-RGB conversion
        }
        return *this;
    }

    CRGBPalette16( const TProgmemRGBPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
            entries[i] =  pgm_read_dword_near( rhs + i);
        }
    }
    CRGBPalette16& operator=( const TProgmemRGBPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
            entries[i] =  pgm_read_dword_near( rhs + i);
        }
        return *this;
    }
    
    inline CRGB& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return entries[x];
    }
    inline const CRGB& operator[] (uint8_t x) const __attribute__((always_inline))
    {
        return entries[x];
    }
    
    inline CRGB& operator[] (int x) __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    inline const CRGB& operator[] (int x) const __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    
    operator CRGB*()
    {
        return &(entries[0]);
    }
    
    CRGBPalette16( const CHSV& c1)
    {
        fill_solid( &(entries[0]), 16, c1);
    }
    CRGBPalette16( const CHSV& c1, const CHSV& c2)
    {
        fill_gradient( &(entries[0]), 16, c1, c2);
    }
    CRGBPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3);
    }
    CRGBPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3, c4);
    }
    
    CRGBPalette16( const CRGB& c1)
    {
        fill_solid( &(entries[0]), 16, c1);
    }
    CRGBPalette16( const CRGB& c1, const CRGB& c2)
    {
        fill_gradient_RGB( &(entries[0]), 16, c1, c2);
    }
    CRGBPalette16( const CRGB& c1, const CRGB& c2, const CRGB& c3)
    {
        fill_gradient_RGB( &(entries[0]), 16, c1, c2, c3);
    }
    CRGBPalette16( const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4)
    {
        fill_gradient_RGB( &(entries[0]), 16, c1, c2, c3, c4);
    }
    

};

class CRGBPalette256 {
public:
    CRGB entries[256];
    CRGBPalette256() {};
    CRGBPalette256( const CRGB& c00,const CRGB& c01,const CRGB& c02,const CRGB& c03,
                  const CRGB& c04,const CRGB& c05,const CRGB& c06,const CRGB& c07,
                  const CRGB& c08,const CRGB& c09,const CRGB& c10,const CRGB& c11,
                  const CRGB& c12,const CRGB& c13,const CRGB& c14,const CRGB& c15 )
    {
        CRGBPalette16 p16(c00,c01,c02,c03,c04,c05,c06,c07,
                          c08,c09,c10,c11,c12,c13,c14,c15);
        *this = p16;
    };
    
    CRGBPalette256( const CRGBPalette256& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
    }
    CRGBPalette256& operator=( const CRGBPalette256& rhs)
    {
        memmove8( &(entries[0]), &(rhs.entries[0]), sizeof( entries));
        return *this;
    }
    
    CRGBPalette256( const CHSVPalette256& rhs)
    {
    	for( int i = 0; i < 256; i++) {
	    	entries[i] = rhs.entries[i]; // implicit HSV-to-RGB conversion
    	}
    }
    CRGBPalette256& operator=( const CHSVPalette256& rhs)
    {
    	for( int i = 0; i < 256; i++) {
	    	entries[i] = rhs.entries[i]; // implicit HSV-to-RGB conversion
    	}
        return *this;
    }

    CRGBPalette256( const CRGBPalette16& rhs16)
    {
        UpscalePalette( rhs16, *this);
    }
    CRGBPalette256& operator=( const CRGBPalette16& rhs16)
    {
        UpscalePalette( rhs16, *this);
        return *this;
    }
    
    CRGBPalette256( const TProgmemRGBPalette16& rhs)
    {
        CRGBPalette16 p16(rhs);
        *this = p16;
    }
    CRGBPalette256& operator=( const TProgmemRGBPalette16& rhs)
    {
        CRGBPalette16 p16(rhs);
        *this = p16;
        return *this;
    }
    
    inline CRGB& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return entries[x];
    }
    inline const CRGB& operator[] (uint8_t x) const __attribute__((always_inline))
    {
        return entries[x];
    }
    
    inline CRGB& operator[] (int x) __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    inline const CRGB& operator[] (int x) const __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }

    operator CRGB*()
    {
        return &(entries[0]);
    }
    
    CRGBPalette256( const CHSV& c1)
    {
        fill_solid( &(entries[0]), 256, c1);
    }
    CRGBPalette256( const CHSV& c1, const CHSV& c2)
    {
        fill_gradient( &(entries[0]), 256, c1, c2);
    }
    CRGBPalette256( const CHSV& c1, const CHSV& c2, const CHSV& c3)
    {
        fill_gradient( &(entries[0]), 256, c1, c2, c3);
    }
    CRGBPalette256( const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4)
    {
        fill_gradient( &(entries[0]), 256, c1, c2, c3, c4);
    }
    
    CRGBPalette256( const CRGB& c1)
    {
        fill_solid( &(entries[0]), 256, c1);
    }
    CRGBPalette256( const CRGB& c1, const CRGB& c2)
    {
        fill_gradient_RGB( &(entries[0]), 256, c1, c2);
    }
    CRGBPalette256( const CRGB& c1, const CRGB& c2, const CRGB& c3)
    {
        fill_gradient_RGB( &(entries[0]), 256, c1, c2, c3);
    }
    CRGBPalette256( const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4)
    {
        fill_gradient_RGB( &(entries[0]), 256, c1, c2, c3, c4);
    }
    
};




typedef enum { NOBLEND=0, BLEND=1 } TBlendType;

CRGB ColorFromPalette( const CRGBPalette16& pal,
                       uint8_t index,
                       uint8_t brightness=255,
                       TBlendType blendType=BLEND);

CRGB ColorFromPalette( const CRGBPalette256& pal,
                       uint8_t index,
                       uint8_t brightness=255,
                       TBlendType blendType=NOBLEND );

CHSV ColorFromPalette( const CHSVPalette16& pal,
                       uint8_t index,
                       uint8_t brightness=255,
                       TBlendType blendType=BLEND);

CHSV ColorFromPalette( const CHSVPalette256& pal,
                       uint8_t index,
                       uint8_t brightness=255,
                       TBlendType blendType=NOBLEND );


// Fill a range of LEDs with a sequece of entryies from a palette
template <typename PALETTE> 
void fill_palette(CRGB* L, uint16_t N, uint8_t startIndex, uint8_t incIndex,
                  const PALETTE& pal, uint8_t brightness, TBlendType blendType)
{
    uint8_t colorIndex = startIndex;
    for( uint16_t i = 0; i < N; i++) {
        L[i] = ColorFromPalette( pal, colorIndex, brightness, blendType);
        colorIndex += incIndex;
    }
}

template <typename PALETTE>
void map_data_into_colors_through_palette( 
	uint8_t *dataArray, uint16_t dataCount, 
	CRGB* targetColorArray, 
	const PALETTE& pal, 
	uint8_t brightness=255, 
	uint8_t opacity=255, 
	TBlendType blendType=BLEND)
{
	for( uint16_t i = 0; i < dataCount; i++) {
		uint8_t d = dataArray[i];
		CRGB rgb = ColorFromPalette( pal, d, brightness, blendType);
		if( opacity == 255 ) {
			targetColorArray[i] = rgb;
		} else {
			targetColorArray[i].nscale8( 256 - opacity);
			rgb.nscale8_video( opacity);
			targetColorArray[i] += rgb;
		}
	}
}

#endif
