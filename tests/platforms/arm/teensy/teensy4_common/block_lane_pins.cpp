#include "test.h"

#include "platforms/arm/teensy/teensy4_common/block_lane_pins.h"
#include "platforms/arm/teensy/teensy4_common/block_lane_pins.impl.hpp"

using fl::teensy4BlockLanePins;
using fl::u8;

FL_TEST_CASE("Teensy 4 block lanes: GPIO6 block from pin 1 takes all 16 pins") {
    u8 pins[16] = {};
    const u8 expected[16] = {1, 0, 24, 25, 19, 18, 14, 15, 17, 16, 22, 23, 20, 21, 26, 27};
    FL_CHECK(teensy4BlockLanePins(1, 16, pins) == 16);
    for (int i = 0; i < 16; ++i) {
        FL_CHECK(pins[i] == expected[i]);
    }
}

FL_TEST_CASE("Teensy 4 block lanes: stop after block terminator 27") {
    u8 pins[16] = {};
    const u8 expected[12] = {19, 18, 14, 15, 17, 16, 22, 23, 20, 21, 26, 27};
    FL_CHECK(teensy4BlockLanePins(19, 16, pins) == 12);
    for (int i = 0; i < 12; ++i) {
        FL_CHECK(pins[i] == expected[i]);
    }
}

FL_TEST_CASE("Teensy 4 block lanes: GPIO7 block honours lane count") {
    u8 pins[4] = {};
    const u8 expected[4] = {10, 12, 11, 13};
    FL_CHECK(teensy4BlockLanePins(10, 4, pins) == 4);
    for (int i = 0; i < 4; ++i) {
        FL_CHECK(pins[i] == expected[i]);
    }
}

FL_TEST_CASE("Teensy 4 block lanes: pin 37 block") {
    u8 pins[8] = {};
    const u8 expected[8] = {37, 36, 35, 34, 39, 38, 28, 31};
    FL_CHECK(teensy4BlockLanePins(37, 8, pins) == 8);
    for (int i = 0; i < 8; ++i) {
        FL_CHECK(pins[i] == expected[i]);
    }
}

FL_TEST_CASE("Teensy 4 block lanes: unknown first pin returns 0") {
    u8 pins[4] = {};
    FL_CHECK(teensy4BlockLanePins(2, 4, pins) == 0);
}
