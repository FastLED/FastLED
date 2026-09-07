/// @file cycle_type.cpp
/// @brief Host tests for the shared ns->cycles conversion.

#include "test.h"

#include "fl/stl/static_assert.h"
#include "platforms/cycle_type.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// The exact form the platform delay headers used before FastLED#4203, kept
// here as the oracle the fast path must match bit for bit.
u32 reference_cycles_from_ns(u32 ns, u32 hz) {
    return static_cast<u32>(((fl::u64)ns * (fl::u64)hz + 999999999ULL)
                            / 1000000000ULL);
}

constexpr u32 kClocks[] = {
    8000000u,   16000000u,  48000000u,  80000000u,  120000000u,
    125000000u, 133000000u, 150000000u, 160000000u, 168000000u,
    180000000u, 240000000u, 300000000u, 480000000u, 600000000u,
    1000000000u,
};

}  // namespace

FL_TEST_CASE("cycles_from_ns matches the exact 64-bit form on the fast path") {
    // The fast path trades the 64-bit divide for 32-bit arithmetic; it is only
    // a valid trade if it is exactly equal, not merely close.
    for (u32 hz : kClocks) {
        for (u32 ns = 0; ns <= 4100u; ++ns) {
            FL_REQUIRE_EQ(cycles_from_ns(ns, hz),
                          reference_cycles_from_ns(ns, hz));
        }
    }
}

FL_TEST_CASE("cycles_from_ns matches beyond the fast-path bound") {
    for (u32 hz : kClocks) {
        for (u32 ns : {5000u, 10000u, 20000u, 100000u, 1000000u, 100000000u}) {
            FL_REQUIRE_EQ(cycles_from_ns(ns, hz),
                          reference_cycles_from_ns(ns, hz));
        }
    }
}

FL_TEST_CASE("cycles_from_ns fast path cannot overflow u32") {
    // ns * (hz / 1000) + 999999 must stay under 2^32-1 everywhere the fast
    // path is taken. An overflow here would silently produce a far too short
    // delay rather than a compile or runtime error.
    constexpr fl::u64 kU32Max = 4294967295ULL;
    for (u32 hz : kClocks) {
        const fl::u64 product =
            static_cast<fl::u64>(4000u) * (hz / 1000u) + 999999ULL;
        FL_REQUIRE(product <= kU32Max);
    }
}

FL_TEST_CASE("cycles_from_ns rounds up and is usable in a constant expression") {
    // Round-up matters: a truncating conversion under-delays every clockless
    // bit, which is the failure mode the callers cannot tolerate.
    FL_CHECK_EQ(cycles_from_ns(400u, 150000000u), 60u);
    FL_CHECK_EQ(cycles_from_ns(1u, 150000000u), 1u);
    FL_CHECK_EQ(cycles_from_ns(0u, 150000000u), 0u);
    FL_STATIC_ASSERT(cycles_from_ns(400u, 150000000u) == 60u,
                     "must fold at compile time for delayNanoseconds<NS>()");
}

}  // FL_TEST_FILE
