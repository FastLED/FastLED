/// @file    Rgb8Video.ino
/// @brief   Animated rainbows optimized for video display on WS2812 LEDs using
/// toVideoRGB_8bit().
/// @example Rgb8Video.ino

#include "FastLED.h"

using namespace fl;

// Rgb8Video
// Animated, ever-changing rainbows optimized for video display.
// Uses CRGB::toVideoRGB_8bit() to boost saturation for better LED display.
// Based on Pride2015 by Mark Kriegsman

#define DATA_PIN 3
#define DATA_PIN_2 4 // Second strip for original RGB comparison
// #define CLK_PIN   4
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS_PER_STRIP 100
#define NUM_STRIPS 2
#define TOTAL_LEDS (NUM_LEDS_PER_STRIP * NUM_STRIPS) // Two strips combined
#define BRIGHTNESS 255

// Combined LED array for both strips
CRGB leds[TOTAL_LEDS];

// fl::ScreenMap screenmap(lut.data(), lut.size());
fl::XYMap xyMap =
    fl::XYMap::constructRectangularGrid(NUM_LEDS_PER_STRIP, NUM_STRIPS);

void setup() {

    // tell FastLED about the LED strip configuration
    // First strip (original RGB) - indices 0-199
    // fl::ScreenMap screenmap(TOTAL_LEDS);

    FastLED.addLeds<LED_TYPE, DATA_PIN_2, COLOR_ORDER>(leds, TOTAL_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xyMap);
    // set master brightness control
    FastLED.setBrightness(BRIGHTNESS);
}

// Animated rainbow wave effect optimized for video display
void rainbowWave() {
    static uint16_t time = 0;
    static uint8_t hueOffset = 0;

    time += 2;
    hueOffset += 1;



    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        // Create a wave pattern using sine function
        uint8_t wave = sin8(time + (i * 8));

        // Calculate hue based on position and time for rainbow effect
        uint8_t hue = hueOffset + (i * 255 / NUM_LEDS_PER_STRIP);

        // Use wave for both saturation and brightness variation
        uint8_t sat = 255 - (wave / 4); // Subtle saturation variation
        uint8_t bri = 128 + (wave / 2); // Brightness wave from 128 to 255

        // Create the original color using HSV
        CRGB original_color = CHSV(hue, sat, bri);

        // Store original color in first strip
        // leds_original[i] = original_color;

        // Convert to video RGB for better display on WS2812 LEDs and store
        // in second strip
        // leds[i] = original_color.toVideoRGB_8bit();
        leds[xyMap(i, 0)] = original_color;
        leds[xyMap(i, 1)] = original_color.toVideoRGB_8bit();
    }
}

void loop() {
    rainbowWave();
    FastLED.show();
}
