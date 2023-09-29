/// @file    Apa102HD.ino
/// @brief   Example showing how to use the APA102HD gamma correction.
/// @example Apa102HD.ino

#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS  20

static bool gamma_function_hit = false;
CRGB leds_hd[NUM_LEDS] = {0};  // HD mode implies gamma.
CRGB leds[NUM_LEDS] = {0};     // Software gamma mode.


uint8_t gamma8(uint8_t x) {
    return uint8_t((uint16_t(x) * x) >> 8);
}

CRGB gammaCorrect(CRGB c) {
    c.r = gamma8(c.r);
    c.g = gamma8(c.g);
    c.b = gamma8(c.b);
    return c;
}

void setup() {
    delay(500); // power-up safety delay
    FastLED.addLeds<APA102HD, 1, 2, RGB>(leds_hd, NUM_LEDS);
    FastLED.addLeds<APA102, 3, 4, RGB>(leds, NUM_LEDS);
}

void loop() {
    uint32_t now = millis();
    uint32_t t = now / 100;
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = sin8(t + i);
        CRGB c(brightness, brightness, brightness);
        leds_hd[i] = c;
        leds[i] = gammaCorrect(c);
    }
    FastLED.show();
    delay(8);
    if (!gamma_function_hit) {
        Serial.println("gamma function not hit");
    }
}

