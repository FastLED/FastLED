#include "FastLED.h"

#include <stdio.h>

extern "C" void app_main() {
    // FastLED retains the array after app_main returns.
    static CRGB leds[3];
    FastLED.addLeds<WS2812, 2, GRB>(leds, 3);
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Blue;
    FastLED.show();
    printf("Bare ESP-IDF: FastLED.show() returned\n");
}
