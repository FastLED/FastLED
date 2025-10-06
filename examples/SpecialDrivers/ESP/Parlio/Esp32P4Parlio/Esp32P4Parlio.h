#pragma once

#ifdef ESP32

#define FASTLED_USES_ESP32P4_PARLIO  // Enable PARLIO driver

#include "FastLED.h"

#define NUM_STRIPS 4
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUM_STRIPS)

// Pin definitions
#define PIN0 1
#define PIN1 2
#define PIN2 3
#define PIN3 4

// LED arrays
CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("FastLED ESP32-P4 PARLIO Driver Demo");
    Serial.println("====================================");

    // Just use FastLED.addLeds like normal!
    // The driver automatically selects the optimal bit width (1/2/4/8/16)
    // based on how many strips you add
    FastLED.addLeds<WS2812, PIN0, GRB>(leds + (0 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN1, GRB>(leds + (1 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN2, GRB>(leds + (2 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN3, GRB>(leds + (3 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);

    FastLED.setBrightness(32);

    Serial.println("\nReady!");
}

void fill_rainbow_all_strips(CRGB* all_leds) {
    static int s_offset = 0;
    for (int j = 0; j < NUM_STRIPS; j++) {
        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
            int idx = i + NUM_LEDS_PER_STRIP * j;
            all_leds[idx] = CHSV((i + s_offset) & 0xFF, 255, 255);
        }
    }
    s_offset++;
}

void loop() {
    fill_rainbow_all_strips(leds);
    FastLED.show();  // Magic happens here!
}

#else  // ESP32

// Non-ESP32 platform
#include "FastLED.h"

#define NUM_LEDS 16
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 7);
    FastLED.show();
    delay(50);
}

#endif  // ESP32
