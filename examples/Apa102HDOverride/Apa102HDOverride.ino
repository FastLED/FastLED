/// @file    Apa102HD.ino
/// @brief   Example showing how to use the APA102HD gamma correction with user override.

#define FASTLED_FIVE_BIT_HD_BITSHIFT_FUNCTION_OVERRIDE

#include <Arduino.h>
#include <FastLED.h>
#include <lib8tion.h>

#define NUM_LEDS 20
// uint8_t DATA_PIN, uint8_t CLOCK_PIN,

#define STRIP_0_DATA_PIN 1
#define STRIP_0_CLOCK_PIN 2
#define STRIP_1_DATA_PIN 3
#define STRIP_1_CLOCK_PIN 4

void five_bit_hd_gamma_bitshift(CRGB colors,
                                CRGB scale,
                                uint8_t global_brightness,
                                CRGB* out_colors,
                                uint8_t *out_power_5bit) {
    // all 0 values for output
    *out_colors = CRGB(0, 0, 0);
    *out_power_5bit = 0;
    Serial.println("Override function called");
}

CRGB leds_hd[NUM_LEDS] = {0}; // HD mode implies gamma.


void setup() {
    delay(500); // power-up safety delay
    // Two strips of LEDs, one in HD mode, one in software gamma mode.
    FastLED.addLeds<APA102HD, STRIP_0_DATA_PIN, STRIP_0_CLOCK_PIN, RGB>(
        leds_hd, NUM_LEDS);
}

void loop() {
    // Draw a a linear ramp of brightnesses to showcase the difference between
    // the HD and non-HD mode.
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = map(i, 0, NUM_LEDS - 1, 0, 255);
        CRGB c(brightness, brightness,
               brightness); // Just make a shade of white.
        leds_hd[i] = c;     // The APA102HD leds do their own gamma correction.
    }
    FastLED.show(); // All leds are now written out.
    delay(8);       // Wait 8 milliseconds until the next frame.
}
