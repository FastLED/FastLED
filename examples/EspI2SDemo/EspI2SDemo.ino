#ifdef ESP32  // This example is only for the ESP32

#include "esp32-hal.h"

#if !(CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32)
// this is only for ESP32 and ESP32-S3
#else

#define NUM_LEDS_PER_STRIP 32

#include <FastLED.h>
#include "platforms/esp/32/yvez_i2s.h"

// Note that this isn't the final api and this is intentionally
// overly strict. The final api will be more flexible.
YvezI2S::CRGBArray6Strips leds;
YvezI2S* i2s = nullptr;

void Blink(CRGB color, int times);

void setup() {
    // TODO: Implement
    i2s = new YvezI2S(&leds, 22, 23);
}

void loop() {
    Blink(CRGB(4, 0, 0), 1);
    Blink(CRGB(0, 4, 0), 2);
    Blink(CRGB(0, 0, 4), 3);
    delay(1000);
}


/// Implementations

void Fill(CRGB color) {
    CRGB* start = leds.get();
    size_t numLeds = leds.size();
    for (size_t i = 0; i < numLeds; i++) {
        start[i] = color;
    }
}

void Blink(CRGB color, int times) {
    for (int i = 0; i < times; i++) {
        Fill(color);
        i2s->showPixels();
        delay(250);
        Fill(CRGB::Black);
        i2s->showPixels();
        delay(250);
    }
}

#endif  // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
#endif  // ESP32