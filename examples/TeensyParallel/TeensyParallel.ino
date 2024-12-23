
// This is an prototype example for the ObjectFLED library for massive pins on
// teensy40/41.

#if !defined(__IMXRT1062__) // Teensy 4.0/4.1 only.
void setup() {}
void loop() {}
#else

#include "platforms/arm/k20/clockless_objectfled.h"

#include <FastLED.h>
#include "fl/warn.h"

#include <iostream>

using namespace fl;

#define PIN_FIRST 3

ClocklessController_ObjectFLED_WS2812<PIN_FIRST, GRB> driver;
#define NUM_LEDS (22 * 22)
CRGB leds[NUM_LEDS];

void wait_for_serial() {
    uint32_t end_timeout = millis() + 1000;
    while (!Serial && end_timeout < millis()) {}
}

void print_startup_info() {
    Serial.println("Start");
    Serial.print("*********************************************\n");
    Serial.print("* TeensyParallel.ino                        *\n");
    Serial.print("*********************************************\n");
    Serial.printf(
        "CPU speed: %d MHz   Temp: %.1f C  %.1f F   Serial baud: %.1f MHz\n",
        F_CPU_ACTUAL / 1000000, tempmonGetTemp(),
        tempmonGetTemp() * 9.0 / 5.0 + 32, 800000 * 1.6 / 1000000.0);
}


void setup() {
    Serial.begin(115200);
    wait_for_serial();
    CFastLED::addLeds(&driver, leds, NUM_LEDS);
}

void fill(CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
}

void blink(CRGB color, int times, int delay_ms = 250) {
    for (int i = 0; i < times; ++i) {
        fill(color);
        FastLED.show();
        delay(delay_ms);
        fill(CRGB::Black);
        FastLED.show();
        delay(delay_ms);
    }
}

void loop() {
    blink(CRGB::Red, 1);
    blink(CRGB::Green, 2);
    blink(CRGB::Blue, 3);
    delay(500);
}

#endif //  __IMXRT1062__
