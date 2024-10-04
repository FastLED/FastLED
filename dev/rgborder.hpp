#include "fx/pride2015.hpp"

#define DATA_PIN 2
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 200
#define BRIGHTNESS 128

#define DELAY 500

CRGB leds[NUM_LEDS];

void fill(CRGB c) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = c;
    }
}

void Blink(CRGB color, int count) {
    for (int i = 0; i < count; i++) {
        fill(color);
        FastLED.show();
        delay(DELAY); // On for 500ms
        fill(CRGB::Black);
        FastLED.show();
        delay(DELAY); // Off for 500ms
    }
    delay(DELAY * 2); // Pause for 1s
}

void setup() {
    // tell FastLED about the LED strip configuration
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setDither(BRIGHTNESS < 255);

    // set master brightness control
    FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
    // Blink red once,
    Blink(CRGB::Red, 1);
    // Blink green twice
    Blink(CRGB::Green, 2);
    // Blink blue three times
    Blink(CRGB::Blue, 3);
}
