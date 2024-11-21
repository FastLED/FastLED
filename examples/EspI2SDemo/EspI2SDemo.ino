#ifdef ESP32  // This example is only for the ESP32

#include "esp32-hal.h"

#if !(CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32)
// this is only for ESP32 and ESP32-S3
#else

#include <FastLED.h>
#include "platforms/esp/32/yvez_i2s.h"

#define CLOCK_PIN 22
#define LATCH_PIN 23

// Note that this isn't the final api and this is intentionally
// overly strict. The final api will be more flexible.
YvezI2S::CRGBArray6Strips leds;  // 256 leds per strip, 6 strips. Mandatory, for now.
YvezI2S::Pins pins({9,10,12,8,18,17});  // Esp32s3 pins from examples.

YvezI2S i2s(&leds, CLOCK_PIN, LATCH_PIN, pins);

void BlinkAndDraw(CRGB color, int times);

void setup() {
}

void loop() {
    BlinkAndDraw(CRGB(4, 0, 0), 1);
    BlinkAndDraw(CRGB(0, 4, 0), 2);
    BlinkAndDraw(CRGB(0, 0, 4), 3);
    delay(1000);
}





/// Helper function definitions.

void Fill(CRGB color) {
    CRGB* start = leds.get();
    size_t numLeds = leds.size();
    for (size_t i = 0; i < numLeds; i++) {
        start[i] = color;
    }
}

void BlinkAndDraw(CRGB color, int times) {
    for (int i = 0; i < times; i++) {
        Fill(color);
        i2s.showPixels();
        delay(250);
        Fill(CRGB::Black);
        i2s.showPixels();
        delay(250);
    }
}

#endif  // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
#endif  // ESP32