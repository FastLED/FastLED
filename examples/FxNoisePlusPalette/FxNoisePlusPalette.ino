/// @file    FxNoisePlusPalette.ino
/// @brief   Noise plus palette effect with XYMap
/// @example FxNoisePlusPalette.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Don't compile this for AVR microcontrollers (like Arduino Uno) because they typically 
// don't have enough memory to handle this complex animation.
// Instead, we provide empty setup/loop functions so the sketch will compile but do nothing.
void setup() {}
void loop() {}
#else  // For all other platforms with more memory (ESP32, Teensy, etc.)
#include "fx/2d/noisepalette.h"
#include "fl/ui.h"

using namespace fl;

#define LED_PIN 3
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

#if __EMSCRIPTEN__
#define GRID_SERPENTINE 0
#else
#define GRID_SERPENTINE 1
#endif

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// This example combines two features of FastLED to produce a remarkable range
// of effects from a relatively small amount of code.  This example combines
// FastLED's color palette lookup functions with FastLED's Perlin noise
// generator, and the combination is extremely powerful.
//
// You might want to look at the "ColorPalette" and "Noise" examples separately
// if this example code seems daunting.
//
//
// The basic setup here is that for each frame, we generate a new array of
// 'noise' data, and then map it onto the LED matrix through a color palette.
//
// Periodically, the color palette is changed, and new noise-generation
// parameters are chosen at the same time.  In this example, specific
// noise-generation values have been selected to match the given color palettes;
// some are faster, or slower, or larger, or smaller than others, but there's no
// reason these parameters can't be freely mixed-and-matched.
//
// In addition, this example includes some fast automatic 'data smoothing' at
// lower noise speeds to help produce smoother animations in those cases.
//
// The FastLED built-in color palettes (Forest, Clouds, Lava, Ocean, Party) are
// used, as well as some 'hand-defined' ones, and some proceedurally generated
// palettes.

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.
// The higher the value of scale, the more "zoomed out" the noise iwll be.  A
// value of 1 will be so zoomed in, you'll mostly see solid colors.

UISlider SCALE("SCALE", 20, 1, 100, 1);

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward. Try
// 1 for a very slow moving effect, or 60 for something that ends up looking
// like water.
UISlider SPEED("SPEED", 30, 1, 60, 1);

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT, GRID_SERPENTINE);
NoisePalette noisePalette(xyMap);

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(96);
    noisePalette.setSpeed(SPEED);
    noisePalette.setScale(SCALE);
}

void loop() {
    noisePalette.setSpeed(SPEED);
    noisePalette.setScale(SCALE);
    EVERY_N_MILLISECONDS(5000) { noisePalette.changeToRandomPalette(); }

    noisePalette.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
}

#endif  // End of the non-AVR code section
