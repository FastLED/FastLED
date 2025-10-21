// Simple compilation test for performance tracking
#define FASTLED_FORCE_SOFTWARE_PINS
#include "FastLED.h"

// Minimal main to ensure full compilation
int main() {
    CRGB leds[1];
    FastLED.addLeds<NEOPIXEL, 3>(leds, 1);
    return 0;
}
