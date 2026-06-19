// Unit tests for fl::number -- 64-bit packed tagged numeric value.
// See FastLED #3022.

#include "test.h"

#include "fl/math/number.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("fl::number default construct is zero/SIGNED_INT") {
    number n;
    FL_CHECK_EQ(n.raw, 0u);
    FL_CHECK(n.get_tag() == number::tag::SIGNED_INT);
    FL_CHECK_EQ(n.payload(), 0u);
}

FL_TEST_CASE("fl::number from_signed round-trip") {
    const i64 sample_values[] = {
        0, 1, -1, 42, -42,
        2147483647LL, -2147483648LL,
        (i64(1) << 50), -(i64(1) << 50),
        (i64(1) << 60) - 1,  // max positive in 61 bits
        -(i64(1) << 60),     // min negative in 61 bits
    };
    for (i64 v : sample_values) {
        number n = number::from_signed(v);
        FL_CHECK(n.get_tag() == number::tag::SIGNED_INT);
        FL_CHECK_EQ(n.as_signed_payload(), v);
    }
}

FL_TEST_CASE("fl::number from_signed saturates out-of-range") {
    // Above max (61 bits signed): saturates to (2^60 - 1).
    number n_above = number::from_signed((i64(1) << 60) + 5);
    FL_CHECK(n_above.get_tag() == number::tag::SIGNED_INT);
    FL_CHECK_EQ(n_above.as_signed_payload(), (i64(1) << 60) - 1);

    // Below min: saturates to -(2^60).
    number n_below = number::from_signed(-(i64(1) << 60) - 5);
    FL_CHECK(n_below.get_tag() == number::tag::SIGNED_INT);
    FL_CHECK_EQ(n_below.as_signed_payload(), -(i64(1) << 60));
}

FL_TEST_CASE("fl::number from_unsigned round-trip + saturation") {
    const u64 sample_values[] = {
        0u, 1u, 42u,
        (u64(1) << 32),
        (u64(1) << 60),
        (u64(1) << 61) - 1,  // max in 61 bits unsigned
    };
    for (u64 v : sample_values) {
        number n = number::from_unsigned(v);
        FL_CHECK(n.get_tag() == number::tag::UNSIGNED_INT);
        FL_CHECK_EQ(n.payload(), v);
    }

    // Above max: saturates.
    number n_sat = number::from_unsigned(u64(-1));
    FL_CHECK(n_sat.get_tag() == number::tag::UNSIGNED_INT);
    FL_CHECK_EQ(n_sat.payload(), (u64(1) << 61) - 1);
}

FL_TEST_CASE("fl::number storage footprint is exactly 64 bits") {
    FL_CHECK_EQ(sizeof(number), sizeof(u64));
}

} // FL_TEST_FILE
