/// @file tests/platforms/cpu_frequency.cpp
/// Covers the FL_CPU_FREQUENCY() rename and its deprecated alias.

#include "test.h"

#include "platforms/cpu_frequency.h"

FL_TEST_CASE("FL_CPU_FREQUENCY reports a plausible clock") {
    // Every branch of the header resolves to a positive Hz value. The host
    // build lands on the F_CPU or 16 MHz fallback; the point is that the macro
    // expands to something usable rather than a stray identifier.
    const fl::u32 hz = (fl::u32)FL_CPU_FREQUENCY();
    FL_CHECK(hz > 0);
    // 1 MHz floor: below this the ns->cycle math in fastled_delay.h collapses
    // to zero-cycle delays, which would silently break every clockless driver.
    FL_CHECK(hz >= 1000000UL);
}

FL_TEST_CASE("deprecated GET_CPU_FREQUENCY still resolves to the same value") {
    // The old spelling is reachable from user sketches via platforms.h. If the
    // alias ever drifts from the real macro, sketches would compile but run at
    // the wrong clock -- a silent timing bug rather than a build error.
    FL_CHECK((fl::u32)GET_CPU_FREQUENCY() == (fl::u32)FL_CPU_FREQUENCY());
}
