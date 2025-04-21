// test_transition_ramp.cpp
// g++ --std=c++11 test_transition_ramp.cpp

#include "fl/namespace.h"
#include "fl/time_alpha.h"
#include "test.h"
#include <cstdint>

using namespace fl;

TEST_CASE("Test transition ramp") {
    // total latch = 100 ms, ramp‑up = 10 ms, ramp‑down = 10 ms
    TimeRamp ramp(10, 100, 10);
    uint32_t t0 = 0;
    ramp.trigger(t0);

    // at start: still at zero
    REQUIRE(ramp.update8(t0) == 0);

    // mid‑rise: 5 ms → (5*255/10) ≈ 127
    REQUIRE(ramp.update8(t0 + 5) == static_cast<uint8_t>((5 * 255) / 10));

    // end of rise: 10 ms → full on
    REQUIRE(ramp.update8(t0 + 10) == 255);

    // plateau: well within [10, 90) ms
    REQUIRE(ramp.update8(t0 + 50) == 255);

    // mid‑fall: elapsed=95 ms → fallingElapsed=5 ms → 255 - (5*255/10) ≈ 128
    uint8_t value = ramp.update8(t0 + 115);
    uint8_t expected = static_cast<uint8_t>(255 - (5 * 255) / 10);
    REQUIRE_EQ(expected, value);

    // after latch: 110 ms → off
    // REQUIRE(ramp.update8(t0 + 110) == 0);

    // now do it again
    ramp.trigger(200);
    // at start: still at zero
    REQUIRE(ramp.update8(200) == 0);
    // mid‑rise: 205 ms → (5*255/10) ≈ 127
    REQUIRE(ramp.update8(205) == static_cast<uint8_t>((5 * 255) / 10));
    // end of rise: 210 ms → full on
    REQUIRE(ramp.update8(210) == 255);
    // plateau: well within [210, 290) ms
    REQUIRE(ramp.update8(250) == 255);
    // mid‑fall: elapsed=295 ms → fallingElapsed=5 ms → 255 - (5*255/10) ≈ 128
    REQUIRE(ramp.update8(315) == static_cast<uint8_t>(255 - (5 * 255) / 10));
    // after latch: 310 ms → off
    REQUIRE(ramp.update8(320) == 0);
    // after latch: 410 ms → off
    REQUIRE(ramp.update8(410) == 0);
}

TEST_CASE("Real world Bug") {
    TimeRamp transition = TimeRamp(500, 0, 500);

    uint8_t value = transition.update8(0);
    CHECK(value == 0);
    value = transition.update8(1);
    CHECK(value == 0);

    transition.trigger(6900);
    value = transition.update8(6900);
    REQUIRE_EQ(value, 0);

    value = transition.update8(6900 + 500);
    REQUIRE_EQ(value, 255);

    value = transition.update8(6900 + 250);
    REQUIRE_EQ(value, 127);
}