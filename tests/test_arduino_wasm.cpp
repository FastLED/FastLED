
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "platforms/wasm/compiler/Arduino.h"

TEST_CASE("arduino_wasm") {
    SUBCASE("random") {
        for (int i = 0; i < 100; i++) {
            long r = random(0, 1);
            CHECK_EQ(0, r);
        }
    }
}

