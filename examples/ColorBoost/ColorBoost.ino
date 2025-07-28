/// @file    Rgb8Video.ino
/// @brief   Animated rainbows optimized for video display on WS2812 LEDs using
/// toVideoRGB_8bit().
/// @example Rgb8Video.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include "FastLED.h"
#include "fl/ease.h"

using namespace fl;

UITitle title("ColorBoost");
UIDescription description("ColorBoost is a function that boosts the saturation of a color without decimating the color from 8 bit -> gamma -> 8 bit (leaving only 8 colors for each component). Use the dropdown menus to select different easing functions for saturation and luminance. Use legacy gfx mode (?gfx=0) for best results.");

UISlider satSlider("Saturation", 60, 0, 255, 1);

// Create dropdown with descriptive ease function names
fl::string easeOptions[] = {
    "None", 
    "In Quad", 
    "Out Quad", 
    "In-Out Quad", 
    "In Cubic", 
    "Out Cubic", 
    "In-Out Cubic", 
    "In Sine", 
    "Out Sine", 
    "In-Out Sine"
};
UIDropdown saturationFunction("Saturation Function", easeOptions);
UIDropdown luminanceFunction("Luminance Function", easeOptions);

// Group related color boost UI elements using UIGroup template multi-argument constructor
UIGroup colorBoostControls("Color Boost", satSlider, saturationFunction, luminanceFunction);

// Rgb8Video
// Animated, ever-changing rainbows optimized for video display.
// Uses CRGB::toVideoRGB_8bit() to boost saturation for better LED display.
// Based on Pride2015 by Mark Kriegsman

#define DATA_PIN 3
#define DATA_PIN_2 4 // Second strip for original RGB comparison
// #define CLK_PIN   4
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS_PER_STRIP 16
#define NUM_STRIPS 16 // Changed from 2 to 100 for 100x100 matrix
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

    // Set default dropdown selections
    saturationFunction.setSelectedIndex(1); // "In Quad"
    luminanceFunction.setSelectedIndex(0);  // "None"
}

EaseType getEaseType(int value) {
    switch (value) {
        case 0: return EASE_NONE;
        case 1: return EASE_IN_QUAD;
        case 2: return EASE_OUT_QUAD;
        case 3: return EASE_IN_OUT_QUAD;
        case 4: return EASE_IN_CUBIC;
        case 5: return EASE_OUT_CUBIC;
        case 6: return EASE_IN_OUT_CUBIC;
        case 7: return EASE_IN_SINE;
        case 8: return EASE_OUT_SINE;
        case 9: return EASE_IN_OUT_SINE;
    }
    FL_ASSERT(false, "Invalid ease type");
    return EASE_NONE;
}

// Animated rainbow wave effect optimized for video display
void rainbowWave() {
    // Use millis() for consistent timing across different devices
    // Scale down millis() to get appropriate animation speed
    uint16_t time = millis() / 16;  // Adjust divisor to control wave speed
    uint8_t hueOffset = millis() / 32; // Adjust divisor to control hue rotation speed

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
            if (y > NUM_STRIPS / 3 * 2) {
                // Upper half - original colors
                leds[xyMap(x, y)] = original_color;
            } else if (y > NUM_STRIPS / 3) {
                // Middle half - transformed colors
                EaseType sat_ease = getEaseType(saturationFunction.as_int());
                EaseType lum_ease = getEaseType(luminanceFunction.as_int());
                leds[xyMap(x, y)] = original_color.colorBoost(sat_ease, lum_ease);
            } else {
                // Lower half - transformed colors
                float r = original_color.r / 255.f;
                float g = original_color.g / 255.f;
                float b = original_color.b / 255.f;

                r = pow(r, 2.0);
                g = pow(g, 2.0);
                b = pow(b, 2.0);

                r = r * 255.f;
                g = g * 255.f;
                b = b * 255.f;

                leds[xyMap(x, y)] = CRGB(r, g, b);
            }
        }
    }
}

void loop() {
    rainbowWave();
    FastLED.show();
}
