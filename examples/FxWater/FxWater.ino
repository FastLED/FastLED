// Author: sutaburosu

// based on https://web.archive.org/web/20160418004149/http://freespace.virgin.net/hugo.elias/graphics/x_water.htm

#include <FastLED.h>
#include "Arduino.h"
#include "fl/xymap.h"

using namespace fl;

#define WIDTH 32
#define HEIGHT 32
#define NUM_LEDS ((WIDTH) * (HEIGHT))
CRGB leds[NUM_LEDS];

// the water needs 2 arrays each slightly bigger than the screen
#define WATERWIDTH (WIDTH + 2)
#define WATERHEIGHT (HEIGHT + 2)
uint8_t water[2][WATERWIDTH * WATERHEIGHT];

void wu_water(uint8_t * const buf, uint16_t x, uint16_t y, uint8_t bright);
void process_water(uint8_t * src, uint8_t * dst) ;

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(WIDTH, HEIGHT);
}

// from: https://github.com/FastLED/FastLED/pull/202
CRGB MyColorFromPaletteExtended(const CRGBPalette16& pal, uint16_t index, uint8_t brightness, TBlendType blendType) {
  // Extract the four most significant bits of the index as a palette index.
  uint8_t index_4bit = (index >> 12);
  // Calculate the 8-bit offset from the palette index.
  uint8_t offset = (uint8_t)(index >> 4);
  // Get the palette entry from the 4-bit index
  const CRGB* entry = &(pal[0]) + index_4bit;
  uint8_t red1   = entry->red;
  uint8_t green1 = entry->green;
  uint8_t blue1  = entry->blue;

  uint8_t blend = offset && (blendType != NOBLEND);
  if (blend) {
    if (index_4bit == 15) {
      entry = &(pal[0]);
    } else {
      entry++;
    }

    // Calculate the scaling factor and scaled values for the lower palette value.
    uint8_t f1 = 255 - offset;
    red1   = scale8_LEAVING_R1_DIRTY(red1,   f1);
    green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
    blue1  = scale8_LEAVING_R1_DIRTY(blue1,  f1);

    // Calculate the scaled values for the neighbouring palette value.
    uint8_t red2   = entry->red;
    uint8_t green2 = entry->green;
    uint8_t blue2  = entry->blue;
    red2   = scale8_LEAVING_R1_DIRTY(red2,   offset);
    green2 = scale8_LEAVING_R1_DIRTY(green2, offset);
    blue2  = scale8_LEAVING_R1_DIRTY(blue2,  offset);
    cleanup_R1();

    // These sums can't overflow, so no qadd8 needed.
    red1   += red2;
    green1 += green2;
    blue1  += blue2;
  }
  if (brightness != 255) {
    // nscale8x3_video(red1, green1, blue1, brightness);
    nscale8x3(red1, green1, blue1, brightness);
  }
  return CRGB(red1, green1, blue1);
}

// Rectangular grid
XYMap xyMap(WIDTH, HEIGHT, false);

// map X & Y coordinates onto a horizontal serpentine matrix layout
uint16_t XY(uint8_t x, uint8_t y) {
  return xyMap.mapToIndex(x, y);
}

void loop() {
  // swap the src/dest buffers on each frame
  static uint8_t buffer = 0;
  uint8_t * const bufA = &water[buffer][0];
  buffer = (buffer + 1) % 2;
  uint8_t * const bufB = &water[buffer][0];

  // add a moving stimulus
  wu_water(bufA, beatsin16(13, 256, HEIGHT * 256), beatsin16(7, 256, WIDTH * 256), beatsin8(160, 64, 255));

  // animate the water
  process_water(bufA, bufB);


  // display the water effect on the LEDs
  uint8_t * input = bufB + WATERWIDTH - 1;
  static uint16_t pal_offset = 0;
  pal_offset += 256;
  for (uint8_t y = 0; y < HEIGHT; y++) {
    input += 2;
    for (uint8_t x = 0; x < WIDTH; x++) {
      leds[XY(x, y)] = MyColorFromPaletteExtended(RainbowColors_p, pal_offset + (*input++ << 8), 255, LINEARBLEND);
    }
  }
  FastLED.show();
}

void process_water(uint8_t * src, uint8_t * dst) {
  src += WATERWIDTH - 1;
  dst += WATERWIDTH - 1;
  for (uint8_t y = 1; y < WATERHEIGHT - 1; y++) {
    src += 2; dst += 2;
    for (uint8_t x = 1; x < WATERWIDTH - 1; x++) {
      uint16_t t = src[-1] + src[1] + src[-WATERWIDTH] + src[WATERWIDTH];
      t >>= 1;
      if (dst[0] < t)
        dst[0] = t - dst[0];
      else
        dst[0] = 0;

      dst[0] -= dst[0] >> 6;
      src++; dst++;
    }
  }
}

// draw a blob of 4 pixels with their relative brightnesses conveying sub-pixel positioning
void wu_water(uint8_t * const buf, uint16_t x, uint16_t y, uint8_t bright) {
  // extract the fractional parts and derive their inverses
  uint8_t xx = x & 0xff, yy = y & 0xff, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  #define WU_WEIGHT(a, b) ((uint8_t)(((a) * (b) + (a) + (b)) >> 8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)
                  };
  #undef WU_WEIGHT
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t local_x = (x >> 8) + (i & 1);
    uint8_t local_y = (y >> 8) + ((i >> 1) & 1);
    uint16_t xy = WATERWIDTH * local_y + local_x;
    if (xy >= WATERWIDTH * WATERHEIGHT) continue;
    uint16_t this_bright = bright * wu[i];
    buf[xy] = qadd8(buf[xy], this_bright >> 8);
  }
}
