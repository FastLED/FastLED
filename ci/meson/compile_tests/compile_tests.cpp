// One translation unit combines common sketch API contracts without pulling
// in independent .ino globals (whose setup/loop symbols would collide).
#include "FastLED.h"

// Compile a real sketch in this same translation unit. Rename its Arduino
// entry points so this smoke executable retains its own main().
#define setup compile_test_blink_setup
#define loop compile_test_blink_loop
#include "../../../examples/Blink/Blink.ino"
#undef loop
#undef setup

static void exercise_color_and_animation(CRGB* leds) {
    fill_rainbow(leds, 4, 0, 16);
    CRGBPalette16 palette = RainbowColors_p;
    leds[0] = ColorFromPalette(palette, beatsin8(30));
    leds[1] = HeatColor(inoise8(7));
    leds[2] = CHSV(96, 255, 128);
    fadeToBlackBy(leds, 4, 16);
}

int main() {
    CRGB colors[4];
    fill_solid(colors, 4, CRGB::Red);
    exercise_color_and_animation(colors);

    FastLED.addLeds<NEOPIXEL, 3>(colors, 4);
    FastLED.setBrightness(64);
    FastLED.show();

    return colors[0] == CRGB::Black ? 1 : 0;
}
