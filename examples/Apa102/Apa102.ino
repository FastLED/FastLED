// @filter: (board is not stm32f103cb)

#include <Arduino.h>
#include <FastLED.h>
#include <lib8tion.h>

#define NUM_LEDS  20

// Defaults come from the board (platforms/default_pins.h) rather than a
// literal, because a literal is a guess about hardware this sketch cannot
// see -- Adafruit Feather M4 has no pin 2 at all. A sketch or build flag
// defining these first still wins.
#ifndef STRIP_DATA_PIN
#define STRIP_DATA_PIN SPI_PIN_DATA_1
#endif

#ifndef STRIP_CLOCK_PIN
#define STRIP_CLOCK_PIN SPI_PIN_CLOCK_1
#endif

CRGB leds[NUM_LEDS] = {0};     // Software gamma mode.

void setup() {
    delay(500); // power-up safety delay
    // Two strips of LEDs, one in HD mode, one in software gamma mode.
    FastLED.addLeds<APA102, STRIP_DATA_PIN, STRIP_CLOCK_PIN, RGB>(leds, NUM_LEDS);
}

uint8_t wrap_8bit(int i) {
    // Modulo % operator here wraps a large "i" so that it is
    // always in [0, 255] range when returned. For example, if
    // "i" is 256, then this will return 0. If "i" is 257,
    // then this will return 1. No matter how big the "i" is, the
    // output range will always be [0, 255]
    return i % 256;
}

void loop() {
    // Draw a linear ramp of brightnesses to showcase the difference between
    // the HD and non-HD mode.
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = map(i, 0, NUM_LEDS - 1, 0, 255);
        CRGB c(brightness, brightness, brightness);  // Just make a shade of white.
        leds[i] = c;
    }
    FastLED.show();  // All LEDs are now displayed.
    delay(8);  // Wait 8 milliseconds until the next frame.
}
