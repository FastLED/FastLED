/// @file    Rgb8Video.ino
/// @brief   Animated rainbows optimized for video display on WS2812 LEDs using
/// toVideoRGB_8bit().
/// @example Rgb8Video.ino

#include "FastLED.h"

using namespace fl;

UITitle title("Rgb8Video");

UISlider satSlider("Saturation", 60, 0, 255, 1);

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
#define NUM_STRIPS 100 // Changed from 2 to 100 for 100x100 matrix
#define TOTAL_LEDS (NUM_LEDS_PER_STRIP * NUM_STRIPS) // 100x100 = 10,000 LEDs
#define BRIGHTNESS 255

// Combined LED array for both strips
CRGB leds[TOTAL_LEDS];

// fl::ScreenMap screenmap(lut.data(), lut.size());
// fl::ScreenMap screenmap(TOTAL_LEDS);
fl::XYMap xyMap =
    fl::XYMap::constructRectangularGrid(NUM_LEDS_PER_STRIP, NUM_STRIPS);

void setup() {

    // tell FastLED about the LED strip configuration
    // First strip (original RGB) - indices 0-199

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

    // Iterate through the entire 100x100 matrix
    for (uint16_t y = 0; y < NUM_STRIPS; y++) {
        for (uint16_t x = 0; x < NUM_LEDS_PER_STRIP; x++) {
            // Create a wave pattern using sine function based on position and
            // time
            uint8_t wave = sin8(time + (x * 8));

            // Calculate hue based on position and time for rainbow effect
            uint8_t hue = hueOffset + (x * 255 / NUM_LEDS_PER_STRIP);

            // Use wave for both saturation and brightness variation
            // uint8_t sat = 255 - (wave / 4); // Subtle saturation variation
            uint8_t bri = 128 + (wave / 2); // Brightness wave from 128 to 255

            // Create the original color using HSV
            CRGB original_color = CHSV(hue, satSlider.value(), bri);

            // Upper half (rows 0-49): original colors
            // Lower half (rows 50-99): transformed colors using
            // toVideoRGB_8bit()
            if (y > NUM_STRIPS / 2) {
                // Upper half - original colors
                leds[xyMap(x, y)] = original_color;
            } else {
                // Lower half - transformed colors
                leds[xyMap(x, y)] = original_color.toVideoRGB_8bit();
            }
        }
    }
}

void loop() {
    rainbowWave();
    FastLED.show();
}
