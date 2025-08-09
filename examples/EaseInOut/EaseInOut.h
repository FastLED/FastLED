/// @file    EaseInOut.ino
/// @brief   Demonstrates easing functions with visual curve display
/// @example EaseInOut.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>
#include "fl/ease.h"
#include "fl/leds.h"

using namespace fl;

// Matrix configuration
#define MATRIX_WIDTH 100
#define MATRIX_HEIGHT 100
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define DATA_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255


#define MATRIX_SERPENTINE true

// Use LedsXY for splat rendering instead of regular CRGB array
LedsXY<MATRIX_WIDTH, MATRIX_HEIGHT> leds;

// Create XYMap for serpentine 100x100 matrix
XYMap xyMap = XYMap::constructSerpentine(MATRIX_WIDTH, MATRIX_HEIGHT);

UITitle title("EaseInOut");
UIDescription description("Use the xPosition slider to see the ease function curve. Use the Ease Type dropdown to select different easing functions. Use the 16-bit checkbox to toggle between 16-bit (checked) and 8-bit (unchecked) precision.");

// UI Controls
UISlider xPosition("xPosition", 0.0f, 0.0f, 1.0f, 0.01f);

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
UIDropdown easeTypeDropdown("Ease Type", easeOptions);

UICheckbox use16Bit("16-bit", true); // Default checked for 16-bit precision

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
    return EASE_IN_OUT_QUAD;
}

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED Ease16InOutQuad Demo - Simple Curve Visualization");

    // Add LEDs and set screen map
    auto *controller =
        &FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    // Convert XYMap to ScreenMap and set it on the controller
    fl::ScreenMap screenMap = xyMap.toScreenMap();
    screenMap.setDiameter(.5); // Set LED diameter for visualization
    controller->setScreenMap(screenMap);

    // Configure FastLED
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setDither(BRIGHTNESS < 255);

    // Set default dropdown selection to "In-Out Quad" (index 3)
    easeTypeDropdown.setSelectedIndex(3);
}

void loop() {
    // Clear the matrix using fl::clear for LedsXY
    fl::clear(leds);

    // Get the current slider value (0.0 to 1.0)
    float sliderValue = xPosition.value();

    // Map slider value to X coordinate (0 to width-1)
    uint8_t x = map(sliderValue * 1000, 0, 1000, 0, MATRIX_WIDTH - 1);

    // Get the selected ease type using the dropdown index
    EaseType selectedEaseType = getEaseType(easeTypeDropdown.as_int());
    
    uint8_t y;
    if (use16Bit.value()) {
        // Use 16-bit precision
        uint16_t easeInput = map(sliderValue * 1000, 0, 1000, 0, 65535);
        uint16_t easeOutput = ease16(selectedEaseType, easeInput);
        y = map(easeOutput, 0, 65535, 0, MATRIX_HEIGHT - 1);
    } else {
        // Use 8-bit precision
        uint8_t easeInput = map(sliderValue * 1000, 0, 1000, 0, 255);
        uint8_t easeOutput = ease8(selectedEaseType, easeInput);
        y = map(easeOutput, 0, 255, 0, MATRIX_HEIGHT - 1);
    }

    // Draw white dot at the calculated position using splat rendering
    if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) {
        leds(x, y) = CRGB::White;
    }

    FastLED.show();
}
