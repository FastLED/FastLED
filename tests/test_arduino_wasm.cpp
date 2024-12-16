
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "platforms/wasm/compiler/Arduino.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("arduino_wasm") {
    SUBCASE("random") {
        for (int i = 0; i < 100; i++) {
            long r = random(0, 1);
            CHECK_EQ(0, r);
        }
    }
}

