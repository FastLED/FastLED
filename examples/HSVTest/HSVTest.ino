/// @file    HSVTest.ino
/// @brief   Test HSV color space conversions
/// @example HSVTest.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.


#include "FastLED.h"

#define NUM_LEDS 5

#ifndef PIN_DATA
#define PIN_DATA 1  // Universally available pin
#endif

#ifndef PIN_CLOCK
#define PIN_CLOCK 2  // Universally available pin
#endif

CRGB leds[NUM_LEDS];

UISlider hueSlider("Hue", 128, 0, 255, 1);
UISlider saturationSlider("Saturation", 255, 0, 255, 1);
UISlider valueSlider("Value", 255, 0, 255, 1);
UICheckbox autoHue("Auto Hue", true);

// Group related HSV control UI elements using UIGroup template multi-argument constructor
UIGroup hsvControls("HSV Controls", hueSlider, saturationSlider, valueSlider, autoHue);

void setup() {
    //FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    // fl::ScreenMap screenMap(NUM_LEDS);

    FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, BGR>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip);
}

void loop() {

    uint8_t hue = hueSlider.value();
    uint8_t saturation = saturationSlider.value();
    uint8_t value = valueSlider.value();

    if (autoHue.value()) {
        uint32_t now = millis();
        uint32_t hue_offset = (now / 100) % 256;
        hue = hue_offset;
        hueSlider.setValue(hue);
    }

    CHSV hsv(hue, saturation, value);

    leds[0] = hsv2rgb_spectrum(hsv);
    leds[2] = hsv2rgb_rainbow(hsv);
    leds[4] = hsv2rgb_fullspectrum(hsv);

    FastLED.show();
}
