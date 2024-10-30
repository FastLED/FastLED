
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

#define NUM_LEDS 1000
#define DATA_PIN 2
#define CLOCK_PIN 3

CRGB leds[NUM_LEDS];

TEST_CASE("Simple") {
    FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
}

