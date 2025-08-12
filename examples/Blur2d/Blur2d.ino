/// @file    Blur2d.ino
/// @brief   Demonstrates 2D blur effects on LED matrix
/// @example Blur2d.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

// UIDescription: This example shows how to blur a strip of LEDs in 2d.

#include <Arduino.h>
#include <FastLED.h>

#include "fl/ui.h"
#include "fl/xymap.h"


using namespace fl;


#if SKETCH_HAS_LOTS_OF_MEMORY
#define WIDTH 22
#define HEIGHT 22
#else
#define WIDTH 12
#define HEIGHT 12
#endif

#define NUM_LEDS (WIDTH * HEIGHT)
#define BLUR_AMOUNT 172
#define DATA_PIN 2 // Change this to match your LED strip's data pin
#define BRIGHTNESS 255
#define SERPENTINE true

CRGB leds[NUM_LEDS];
uint8_t pos = 0;
bool toggle = false;
XYMap xymap(WIDTH, HEIGHT, SERPENTINE);

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)
      .setScreenMap(xymap);  // Necessary when using the FastLED web compiler to display properly on a web page.
    FastLED.setBrightness(BRIGHTNESS);
    Serial.println("setup");
}

void loop() {
    static int x = random(WIDTH);
    static int y = random(HEIGHT);
    static CRGB c = CRGB(0, 0, 0);
    blur2d(leds, WIDTH, HEIGHT, BLUR_AMOUNT, xymap);
    EVERY_N_MILLISECONDS(1000) {
        x = random(WIDTH);
        y = random(HEIGHT);
        uint8_t r = random(255);
        uint8_t g = random(255);
        uint8_t b = random(255);
        c = CRGB(r, g, b);
    }
    leds[xymap(x, y)] = c;
    FastLED.show();
    delay(20);
}
