#pragma once

#ifdef ESP32  // This example is only for the ESP32



#if !(CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32)
// this is only for ESP32 and ESP32-S3
void init() {}
void setup() {}
#else
#include <FastLED.h>
#include "scoped_ptr.h"
#include "fixed_vector.h"



// printf
#include <stdio.h>

#include <iostream>
#include "esp32-hal.h"

#include "Arduino.h"

#define NBIS2SERIALPINS 6 // the number of virtual pins here mavimum 6x8=48 strips
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8)
#define NUM_STRIPS (NBIS2SERIALPINS * 8)

#include "platforms/esp/32/yves_i2s.h"

#define CLOCK_PIN 46
#define LATCH_PIN 3

// Note that this isn't the final api and this is intentionally
// overly strict. The final api will be more flexible.
// YvesI2S::CRGBArray6Strips leds;  // 256 leds per strip, 6 strips. Mandatory, for now.

CRGB leds[NUM_STRIPS * NUM_LEDS_PER_STRIP] = {0};
FixedVector<int, 6>  pins({9,10,12,8,18,17});  // Esp32s3 pins from examples.

void BlinkAndDraw(CRGB color, int times);


YvesI2S i2s(leds, pins, CLOCK_PIN, LATCH_PIN);

void setup() {
    Serial.begin(115200);
    // FastLED.delay(2000);
    // driver.initled(leds, Pins, CLOCK_PIN, LATCH_PIN);
    i2s.initOnce();
    //i2s = new YvesI2S(leds, CLOCK_PIN, LATCH_PIN);
}

void loop() {
    // std::cout << "loop" << std::endl;
    printf("loop\n");
    BlinkAndDraw(CRGB(4, 0, 0), 1);
    BlinkAndDraw(CRGB(0, 4, 0), 2);
    BlinkAndDraw(CRGB(0, 0, 4), 3);
    /// driver.showPixels();
    i2s.showPixels();
    // FastLED.delay(1000);
    delay(1000);
}


/// Helper function definitions.

void Fill(CRGB color) {
    for (size_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
}

void BlinkAndDraw(CRGB color, int times) {
    for (int i = 0; i < times; i++) {
        Fill(color);
        i2s.showPixels();
        delay(250);
        Fill(CRGB::Black);
        i2s.showPixels();
        delay(250);
    }
}


#endif  // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
#endif  // ESP32