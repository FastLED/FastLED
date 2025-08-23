/// @file    ColorBoost.h
/// @brief   Demo of CRGB::colorBoost() for video display on WS2812 LEDs using animated rainbow effect 
///          (based on Pride2015 by Mark Kriegsman)
/// @example ColorBoost.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

// This demo shows use of CRGB::colorBoost() to boost saturation for better LED display, compared to 
// normal colors and colors adjusted with gamma correction.
// The demo involves animated, ever-changing rainbows (based on Pride2015 by Mark Kriegsman).

#include "FastLED.h"
#include "fl/ease.h"

using namespace fl;

UITitle title("ColorBoost");
UIDescription description("CRGB::colorBoost() is a function that boosts the saturation of a color without decimating the color from 8 bit -> gamma -> 8 bit (leaving only 8 colors for each component). Use the dropdown menus to select different easing functions for saturation and luminance. Use legacy gfx mode (?gfx=0) for best results.");

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

#define DATA_PIN 2
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define WIDTH 22
#define HEIGHT 22 
#define NUM_LEDS (WIDTH * HEIGHT)
#define BRIGHTNESS 150

CRGB leds[NUM_LEDS];

// fl::ScreenMap screenmap(lut.data(), lut.size());
// fl::ScreenMap screenmap(NUM_LEDS);
fl::XYMap xyMap =
    fl::XYMap::constructRectangularGrid(WIDTH, HEIGHT);

void setup() {

    // tell FastLED about the LED strip configuration
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
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

// Animated rainbow wave effect (Pride2015), with matrix divided into three segments to compare:
// - Normal colors (top)
// - Colors optimized using colorBoost() (middle)
// - Colors adjusted using gamma correction (bottom)
void rainbowWave() {
    // Use millis() for consistent timing across different devices
    // Scale down millis() to get appropriate animation speed
    uint16_t time = millis() / 16;  // Adjust divisor to control wave speed
    uint8_t hueOffset = millis() / 32; // Adjust divisor to control hue rotation speed

    // Iterate through the entire matrix
    for (uint16_t y = 0; y < HEIGHT; y++) {
        for (uint16_t x = 0; x < WIDTH; x++) {
            // Create a wave pattern using sine function based on position and time
            uint8_t wave = sin8(time + (x * 8));

            // Calculate hue based on position and time for rainbow effect
            uint8_t hue = hueOffset + (x * 255 / WIDTH);

            // Use wave for both saturation and brightness variation
            // uint8_t sat = 255 - (wave / 4); // Subtle saturation variation
            uint8_t bri = 128 + (wave / 2); // Brightness wave from 128 to 255

            // Create the original color using HSV
            CRGB original_color = CHSV(hue, satSlider.value(), bri);

            if (y > HEIGHT / 3 * 2) {
                // Upper third - original colors
                leds[xyMap(x, y)] = original_color;
            } else if (y > HEIGHT / 3) {
                // Middle third - colors transformed with colorBoost()
                EaseType sat_ease = getEaseType(saturationFunction.as_int());
                EaseType lum_ease = getEaseType(luminanceFunction.as_int());
                leds[xyMap(x, y)] = original_color.colorBoost(sat_ease, lum_ease);
            } else {
                // Lower third - colors transformed using gamma correction
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
