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
//      CRGBPalette16 myPalette =
//      {
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
//      };
//
//    Or you can initiaize your palette with a preset color scheme:
//      SetRainbowStripesPalette( myPalette);
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

typedef CRGB CRGBPalette16[16];
typedef CRGB CRGBPalette256[256];

typedef enum { INTERPOLATION_NONE=0, INTERPOLATION_BLEND=1 } TInterpolationType;

CRGB ColorFromPalette( const CRGBPalette16& pal,
                       uint8_t index,
                       uint8_t brightness=255,
                       TInterpolationType interpolationType=INTERPOLATION_BLEND);

CRGB ColorFromPalette( const CRGBPalette256& pal,
                       uint8_t index,
                       uint8_t brightness=255,
                       TInterpolationType interpolationType=INTERPOLATION_NONE );

// Preset color schemes, such as they are.
// Try Rainbow Stripes or Lava first.
void SetForestPalette(CRGBPalette16& pal);
void SetCloudPalette(CRGBPalette16& pal);
void SetLavaPalette(CRGBPalette16& pal);
void SetOceanPalette(CRGBPalette16& pal);
void SetRainbowPalette(CRGBPalette16& pal);
void SetRainbowStripesPalette(CRGBPalette16& pal);


// Convert a 16-entry palette to a 256-entry palette
void UpscalePalette(const CRGBPalette16& srcpal16, CRGBPalette256& destpal256);

// Fill a range of LEDs with a sequece of entryies from a palette
void fill_palette(CRGB* L, uint16_t N,
                  uint8_t startIndex, uint8_t incIndex,
                  const CRGBPalette16& pal,
                  uint8_t brightness=255,
                  TInterpolationType interpType=INTERPOLATION_BLEND);

void fill_palette(CRGB* L, uint16_t N,
                  uint8_t startIndex, uint8_t incIndex,
                  const CRGBPalette256& pal,
                  uint8_t brightness=255,
                  TInterpolationType interpType=INTERPOLATION_NONE);


#endif
