// test_transition_ramp.cpp
// g++ --std=c++11 test_transition_ramp.cpp

#include "test.h"
#include <cstdint>
#include "fl/transition_ramp.h"
#include "fl/namespace.h"

using namespace fl;

TEST_CASE("Test transition ramp") {
    // total latch = 100 ms, ramp‑up = 10 ms, ramp‑down = 10 ms
    TransitionRamp ramp(100, 10, 10);
    uint32_t t0 = 0;
    ramp.trigger(t0);

    // at start: still at zero
    REQUIRE(ramp.value(t0) == 0);

    // mid‑rise: 5 ms → (5*255/10) ≈ 127
    REQUIRE(ramp.value(t0 + 5) == static_cast<uint8_t>((5 * 255) / 10));

    // end of rise: 10 ms → full on
    REQUIRE(ramp.value(t0 + 10) == 255);

    // plateau: well within [10, 90) ms
    REQUIRE(ramp.value(t0 + 50) == 255);

    // mid‑fall: elapsed=95 ms → fallingElapsed=5 ms → 255 - (5*255/10) ≈ 128
    REQUIRE(ramp.value(t0 + 95) == static_cast<uint8_t>(255 - (5 * 255) / 10));

    // after latch: 110 ms → off
    REQUIRE(ramp.value(t0 + 110) == 0);

    // now do it again
    ramp.trigger(200);
    // at start: still at zero
    REQUIRE(ramp.value(200) == 0);
    // mid‑rise: 205 ms → (5*255/10) ≈ 127
    REQUIRE(ramp.value(205) == static_cast<uint8_t>((5 * 255) / 10));
    // end of rise: 210 ms → full on
    REQUIRE(ramp.value(210) == 255);
    // plateau: well within [210, 290) ms
    REQUIRE(ramp.value(250) == 255);
    // mid‑fall: elapsed=295 ms → fallingElapsed=5 ms → 255 - (5*255/10) ≈ 128
    REQUIRE(ramp.value(295) == static_cast<uint8_t>(255 - (5 * 255) / 10));
    // after latch: 310 ms → off
    REQUIRE(ramp.value(310) == 0);
    // after latch: 410 ms → off
    REQUIRE(ramp.value(410) == 0);
}
