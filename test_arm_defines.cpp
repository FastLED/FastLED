// Simple test to verify ARM define changes work correctly
#include "FastLED.h"

// This should compile successfully on ARM platforms
// and fail with clear error messages if FASTLED_ARM is not defined

int main() {
#ifdef FASTLED_ARM
    // ARM platform detected - this should work
    CRGB leds[10];
    FastLED.addLeds<WS2812, 2>(leds, 10);
    return 0;
#else
    // Non-ARM platform - also should work
    return 0;
#endif
}