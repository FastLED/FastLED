#pragma once

#ifdef ESP32  // This example is only for the ESP32

#include "esp32-hal.h"

#if !(CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32)
// this is only for ESP32 and ESP32-S3
void init() {}
void setup() {}
#else
#include <FastLED.h>
#include "fl/scoped_ptr.h"
#include "fl/vector.h"

#include <stdio.h>
#include <iostream>


#include "platforms/esp/32/yves_i2s.h"

#define CLOCK_PIN 46
#define LATCH_PIN 3

// Note that this isn't the final api and will change.
fl::FixedVector<int, 6>  pins({0,1,2,3,18,17});
// fl::FixedVector<int, 6>  pins({9,10,12,8,18,17});  // Esp32s3 pins from examples.

void BlinkAndDraw(CRGB color, int times);

YvesI2S i2s(pins, CLOCK_PIN, LATCH_PIN);
Slice<CRGB> leds;

void setup() {
    Serial.begin(115200);
    // FastLED.delay(2000);
    leds = i2s.initOnce();
}

void loop() {
    // std::cout << "loop" << std::endl;
    printf("loop2\n");
    BlinkAndDraw(CRGB(4, 0, 0), 1);
    BlinkAndDraw(CRGB(0, 4, 0), 2);
    BlinkAndDraw(CRGB(0, 0, 4), 3);
    delay(1000);
}


/// Helper function definitions.

void Fill(CRGB color) {
    for (size_t i = 0; i < leds.size(); i++) {
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