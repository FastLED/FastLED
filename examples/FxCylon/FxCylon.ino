/// @file    FxCylon.ino
/// @brief   Cylon eye effect with ScreenMap
/// @example FxCylon.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>
#include "fx/1d/cylon.h"
#include "fl/screenmap.h"

using namespace fl;

// How many leds in your strip?
#define NUM_LEDS 64 

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power).
#define DATA_PIN 2

// Define the array of leds
CRGB leds[NUM_LEDS];

// Create a Cylon instance
Cylon cylon(NUM_LEDS);

void setup() {
    ScreenMap screenMap = ScreenMap::DefaultStrip(NUM_LEDS, 1.5f, 0.5f);
    FastLED.addLeds<WS2812,DATA_PIN,RGB>(leds,NUM_LEDS).setRgbw().setScreenMap(screenMap);
    FastLED.setBrightness(84);
}

void loop() { 
    cylon.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    delay(cylon.delay_ms);
}
