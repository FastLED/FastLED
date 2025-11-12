/// @file timeout.cpp
/// @brief Test for Timeout class rollover-safe arithmetic

#include "test.h"
#include "fl/timeout.h"

using namespace fl;

TEST_CASE("Timeout - rollover test") {
    // Test critical rollover scenario: timeout starts before rollover (0xFFFFFFFF),
    // ends after rollover (0x00000000+). This verifies unsigned arithmetic works correctly.

    uint32_t start = 0xFFFFFF00;  // 256 ticks before rollover
    uint32_t duration = 512;       // Duration spans the rollover boundary

    Timeout timeout(start, duration);

    // At start: not done, zero elapsed
    CHECK_FALSE(timeout.done(start));
    CHECK(timeout.elapsed(start) == 0);

    // 256 ticks later: at rollover point (0x00000000), still not done
    uint32_t at_rollover = start + 256;  // = 0x00000000 due to wraparound
    CHECK_FALSE(timeout.done(at_rollover));
    CHECK(timeout.elapsed(at_rollover) == 256);

    // 256 more ticks: past rollover (0x00000100), now done
    uint32_t past_rollover = at_rollover + 256;  // = 0x00000100
    CHECK(timeout.done(past_rollover));
    CHECK(timeout.elapsed(past_rollover) == 512);
}
