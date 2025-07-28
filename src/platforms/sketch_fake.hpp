// Shared fallback implementation for Arduino examples that require specific platforms
// This file provides a common fallback for examples that won't compile on all platforms

#include <FastLED.h>

#define NUM_LEDS 60

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(9600);
    Serial.println("Platform-specific example - running in fallback mode");
    Serial.println("This example requires specific hardware/libraries not available on this platform");
    
    // Use a simple WS2812 setup for fallback
    FastLED.addLeds<WS2812, 2, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(60);
}

void loop() {
    // Simple rainbow animation for fallback
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255/NUM_LEDS);
    FastLED.show();
    delay(50);
    hue += 2;
}
