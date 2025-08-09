/// @file    Fire2023.ino
/// @brief   Enhanced fire effect with ScreenMap
/// @example Fire2023.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

/*This is a fire effect based on the famous Fire2012; but with various small improvements.
Perlin noise is being used to make a fire layer and a smoke layer;
and the overlay of both can make a quite realistic effect.

The speed of both need to be adapted to the matrix size and width: 
* Super small matrices (like 3x3 led) don't need the smoke
* medium sized matrices (8x8 for example) profit from fine tuning both Fire Speed/scale as well as Smoke speed/scale

This code was adapted for a matrix with just four LED columns in 90° around a core and a height of 28.

Right at the bottom of the code, you find a translation matrix that needs to be adapted to your set up. I included
a link to a helpful page for this.

@repo https://github.com/Anderas2/Fire2023
@author https://github.com/Anderas2

Demo: https://www.youtube.com/shorts/a_Wr0q9YQs4
*/


#include "FastLED.h"
#include "fl/xymap.h"
#include "fl/screenmap.h"
#include "fl/vector.h"

using namespace fl;


// matrix size
#define WIDTH  4
#define HEIGHT 28
#define CentreX  (WIDTH / 2) - 1
#define CentreY (HEIGHT / 2) - 1

// NUM_LEDS = WIDTH * HEIGHT
#define PIXELPIN 3  // universal pin that works on all platforms
#define NUM_LEDS      120
#define LAST_VISIBLE_LED 119


// Fire properties
#define BRIGHTNESS 255
#define FIRESPEED 17
#define FLAMEHEIGHT 3.8 // the higher the value, the higher the flame
#define FIRENOISESCALE 125 // small values, softer fire. Big values, blink fire. 0-255

// Smoke screen properties
// The smoke screen works best for big fire effects. It effectively cuts of a part of the flames
// from the rest, sometimes; which looks very much fire-like. For small fire effects with low
// LED count in the height, it doesn't help
// speed must be a little different and faster from Firespeed, to be visible. 
// Dimmer should be somewhere in the middle for big fires, and low for small fires.
#define SMOKESPEED 25 // how fast the perlin noise is parsed for the smoke
#define SMOKENOISE_DIMMER 250 // thickness of smoke: the lower the value, the brighter the flames. 0-255
#define SMOKENOISESCALE 125 // small values, softer smoke. Big values, blink smoke. 0-255

CRGB leds[NUM_LEDS];

// fire palette roughly like matlab "hot" colormap
// This was one of the most important parts to improve - fire color makes fire impression.
// position, r, g, b value.
// max value for "position" is BRIGHTNESS
DEFINE_GRADIENT_PALETTE(hot_gp) {
    27, 0, 0, 0,                     // black
    28, 140, 40, 0,                 // red
    30, 205, 80, 0,              // orange
    155, 255, 100, 0,    
    210, 255, 200, 0,             // yellow
    255, 255, 255, 255             // white
};
CRGBPalette32 hotPalette = hot_gp;

// Map XY coordinates to numbers on the LED strip
uint8_t XY (uint8_t x, uint8_t y);


// parameters and buffer for the noise array
#define NUM_LAYERS 2
// two layers of perlin noise make the fire effect
#define FIRENOISE 0
#define SMOKENOISE 1
uint32_t x[NUM_LAYERS];
uint32_t y[NUM_LAYERS];
uint32_t z[NUM_LAYERS];
uint32_t scale_x[NUM_LAYERS];
uint32_t scale_y[NUM_LAYERS];

uint8_t noise[NUM_LAYERS][WIDTH][HEIGHT];
uint8_t noise2[NUM_LAYERS][WIDTH][HEIGHT];

uint8_t heat[NUM_LEDS];


ScreenMap makeScreenMap();

void setup() {

  //Serial.begin(115200);
  // Adjust this for you own setup. Use the hardware SPI pins if possible.
  // On Teensy 3.1/3.2 the pins are 11 & 13
  // Details here: https://github.com/FastLED/FastLED/wiki/SPI-Hardware-or-Bit-banging
  // In case you see flickering / glitching leds, reduce the data rate to 12 MHZ or less
  auto screenMap = makeScreenMap();
  FastLED.addLeds<NEOPIXEL, PIXELPIN>(leds, NUM_LEDS).setScreenMap(screenMap); // Pin für Neopixel
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setDither(DISABLE_DITHER);
}

void Fire2023(uint32_t now);

void loop() {
  EVERY_N_MILLISECONDS(8) {
    Fire2023(millis());
  }
  FastLED.show();
}

ScreenMap makeScreenMap() {
    fl::vector<vec2f> lut;
    for (uint16_t y = 0; y < WIDTH; y++) {
        for (uint16_t x = 0; x < HEIGHT; x++) {
            vec2f xy = {float(x) * 3, float(y) * 20};
            lut.push_back(xy);
        }
    }
    return ScreenMap(lut.data(), lut.size(), 1);
}

void Fire2023(uint32_t now) {
  // some changing values
  // these values are produced by perlin noise to add randomness and smooth transitions
  uint16_t ctrl1 = inoise16(11 * now, 0, 0);
  uint16_t ctrl2 = inoise16(13 * now, 100000, 100000);
  uint16_t  ctrl = ((ctrl1 + ctrl2) >> 1);

  // parameters for the fire heat map
  x[FIRENOISE] = 3 * ctrl * FIRESPEED;
  y[FIRENOISE] = 20 * now * FIRESPEED;
  z[FIRENOISE] = 5 * now * FIRESPEED;
  scale_x[FIRENOISE] = scale8(ctrl1, FIRENOISESCALE);
  scale_y[FIRENOISE] = scale8(ctrl2, FIRENOISESCALE);

  //calculate the perlin noise data for the fire
  for (uint8_t x_count = 0; x_count < WIDTH; x_count++) {
    uint32_t xoffset = scale_x[FIRENOISE] * (x_count - CentreX);
    for (uint8_t y_count = 0; y_count < HEIGHT; y_count++) {
      uint32_t yoffset = scale_y[FIRENOISE] * (y_count - CentreY);
      uint16_t data = ((inoise16(x[FIRENOISE] + xoffset, y[FIRENOISE] + yoffset, z[FIRENOISE])) + 1);
      noise[FIRENOISE][x_count][y_count] = data >> 8;
    }
  }

  // parameters for the smoke map
  x[SMOKENOISE] = 3 * ctrl * SMOKESPEED;
  y[SMOKENOISE] = 20 * now * SMOKESPEED;
  z[SMOKENOISE] = 5 * now * SMOKESPEED;
  scale_x[SMOKENOISE] = scale8(ctrl1, SMOKENOISESCALE);
  scale_y[SMOKENOISE] = scale8(ctrl2, SMOKENOISESCALE);

  //calculate the perlin noise data for the smoke
  for (uint8_t x_count = 0; x_count < WIDTH; x_count++) {
    uint32_t xoffset = scale_x[SMOKENOISE] * (x_count - CentreX);
    for (uint8_t y_count = 0; y_count < HEIGHT; y_count++) {
      uint32_t yoffset = scale_y[SMOKENOISE] * (y_count - CentreY);
      uint16_t data = ((inoise16(x[SMOKENOISE] + xoffset, y[SMOKENOISE] + yoffset, z[SMOKENOISE])) + 1);
      noise[SMOKENOISE][x_count][y_count] = data / SMOKENOISE_DIMMER;
    }
  }

  //copy everything one line up
  for (uint8_t y = 0; y < HEIGHT - 1; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      heat[XY(x, y)] = heat[XY(x, y + 1)];
    }
  }

  // draw lowest line - seed the fire where it is brightest and hottest
  for (uint8_t x = 0; x < WIDTH; x++) {
    heat[XY(x, HEIGHT-1)] =  noise[FIRENOISE][WIDTH - x][CentreX];
    //if (heat[XY(x, HEIGHT-1)] < 200) heat[XY(x, HEIGHT-1)] = 150; 
  }

  // dim the flames based on FIRENOISE noise. 
  // if the FIRENOISE noise is strong, the led goes out fast
  // if the FIRENOISE noise is weak, the led stays on stronger.
  // once the heat is gone, it stays dark.
  for (uint8_t y = 0; y < HEIGHT - 1; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      uint8_t dim = noise[FIRENOISE][x][y];
      // high value in FLAMEHEIGHT = less dimming = high flames
      dim = dim / FLAMEHEIGHT;
      dim = 255 - dim;
      heat[XY(x, y)] = scale8(heat[XY(x, y)] , dim);

      // map the colors based on heatmap
      // use the heat map to set the color of the LED from the "hot" palette
      //                               whichpalette    position      brightness     blend or not
      leds[XY(x, y)] = ColorFromPalette(hotPalette, heat[XY(x, y)], heat[XY(x, y)], LINEARBLEND);

      // dim the result based on SMOKENOISE noise
      // this is not saved in the heat map - the flame may dim away and come back
      // next iteration.
      leds[XY(x, y)].nscale8(noise[SMOKENOISE][x][y]);

    }
  }
}

/* Physical layout of LED strip ****************************/
uint8_t XY (uint8_t x, uint8_t y) {
  // any out of bounds address maps to the first hidden pixel
  // https://macetech.github.io/FastLED-XY-Map-Generator/
  if ( (x >= WIDTH) || (y >= HEIGHT) ) {
    return (LAST_VISIBLE_LED + 1);
  }
  const uint8_t XYTable[] = {
    25,  26,  81,  82,
    25,  27,  81,  83,  
    25,  28,  80,  84,  
    24,  29,  79,  85,  
    23,  30,  78,  86,  
    22,  31,  77,  87,  
    21,  32,  76,  88,  
    20,  33,  75,  89,  
    19,  34,  74,  90,  
    18,  35,  73,  91,  
    17,  36,  72,  92,  
    16,  37,  71,  93,
    15,  38,  70,  94,
    14,  39,  69,  95,
    13,  40,  68,  96,
    12,  41,  67,  97,
    11,  42,  66,  98,
    10,  43,  65,  99,
     9,  44,  64, 100,	
     8,  45,  63, 101,	
     7,  46,  62, 102,	
     6,  47,  61, 103,	
     5,  48,  60, 104,	
     4,  49,  59, 105,	
     3,  50,  58, 106,
     2,  51,  57, 107,	 
     1,  52,  56, 108,
     0,  53,  55, 109
  };  

  uint8_t i = (y * WIDTH) + x;
  uint8_t j = XYTable[i];
  return j;
}


