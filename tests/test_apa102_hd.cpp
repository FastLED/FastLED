
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"

CRGB leds[1];

TEST_CASE("sanity check") {
  CHECK(1 == 1);
}

