#ifndef __INC_COLORUTILS_H
#define __INC_COLORUTILS_H

#include "pixeltypes.h"


// fill_solid -   fill a range of LEDs with a solid color
void fill_solid( struct CRGB * pFirstLED, int numToFill,
                 const struct CRGB& color);

// fill_rainbow - fill a range of LEDs with a rainbow of colors, at
//                full saturation and full value (brightness)
void fill_rainbow( struct CRGB * pFirstLED, int numToFill,
                   uint8_t initialhue,
                   uint8_t deltahue = 5);


// fill_gradient - fill a range of LEDs with a smooth gradient
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
typedef enum { FORWARD_HUES, BACKWARD_HUES, SHORTEST_HUES, LONGEST_HUES } TGradientDirectionCode;

void fill_gradient( struct CRGB* leds,
                    uint16_t startpos, CHSV startcolor,
                    uint16_t endpos,   CHSV endcolor,
                    TGradientDirectionCode directionCode = SHORTEST_HUES );



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
// Palettes map an 8-bit value (0..255) to an RGB color.
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
typedef prog_uint32_t TProgmemPalette16[16];

// Convert a 16-entry palette to a 256-entry palette
void UpscalePalette(const CRGBPalette16& srcpal16, CRGBPalette256& destpal256);


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
    
    CRGBPalette16( const TProgmemPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; i++) {
            entries[i] =  pgm_read_dword_near( rhs + i);
        }
    }
    CRGBPalette16& operator=( const TProgmemPalette16& rhs)
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
    
    CRGBPalette256( const CRGBPalette16& rhs16)
    {
        UpscalePalette( rhs16, *this);
    }
    CRGBPalette256& operator=( const CRGBPalette16& rhs16)
    {
        UpscalePalette( rhs16, *this);
        return *this;
    }
    
    CRGBPalette256( const TProgmemPalette16& rhs)
    {
        CRGBPalette16 p16(rhs);
        *this = p16;
    }
    CRGBPalette256& operator=( const TProgmemPalette16& rhs)
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


// Fill a range of LEDs with a sequece of entryies from a palette
void fill_palette(CRGB* L, uint16_t N,
                  uint8_t startIndex, uint8_t incIndex,
                  const CRGBPalette16& pal,
                  uint8_t brightness=255,
                  TBlendType blendType=BLEND);

void fill_palette(CRGB* L, uint16_t N,
                  uint8_t startIndex, uint8_t incIndex,
                  const CRGBPalette256& pal,
                  uint8_t brightness=255,
                  TBlendType blendType=NOBLEND);


// Preset color schemes, such as they are.
const TProgmemPalette16 CloudColors_p PROGMEM =
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

const TProgmemPalette16 LavaColors_p PROGMEM =
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


const TProgmemPalette16 OceanColors_p PROGMEM =
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

const TProgmemPalette16 ForestColors_p PROGMEM =
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

const TProgmemPalette16 RainbowColors_p PROGMEM =
{
    0xFF0000,
    0xD52A00,
    0xAB5500,
    0xAB7F00,
    0xABAB00,
    0x56D500,
    0x00FF00,
    0x00D52A,
    0x00AB55,
    0x0056AA,
    0x0000FF,
    0x2A00D5,
    0x5500AB,
    0x7F0081,
    0xAB0055,
    0xD5002B
};

#define RainbowStripesColors_p RainbowStripeColors_p
const TProgmemPalette16 RainbowStripeColors_p PROGMEM =
{
    0xFF0000,
    0x000000,
    0xAB5500,
    0x000000,
    0xABAB00,
    0x000000,
    0x00FF00,
    0x000000,
    0x00AB55,
    0x000000,
    0x0000FF,
    0x000000,
    0x5500AB,
    0x000000,
    0xAB0055,
    0x000000
};

const TProgmemPalette16 PartyColors_p PROGMEM =
{
    0x5500AB,
    0x84007C,
    0xB5004B,
    0xE5001B,
    0xE81700,
    0xB84700,
    0xAB7700,
    0xABAB00,
    0xAB5500,
    0xDD2200,
    0xF2000E,
    0xC2003E,
    0x8F0071,
    0x5F00A1,
    0x2F00D0,
    0x0007F9,
};


#endif
