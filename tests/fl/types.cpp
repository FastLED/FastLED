#include "platforms/cycle_type.h"
#include "doctest.h"
#include "fl/stl/type_traits.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("fl::cycle_t type definition") {
    SUBCASE("cycle_t exists and is signed") {
        cycle_t value = 0;
        CHECK_EQ(value, 0);

        // Test that cycle_t is signed
        cycle_t negative = -1;
        CHECK(negative < 0);
    }

#if defined(__AVR__)
    SUBCASE("cycle_t is int on AVR platforms") {
        // On AVR, cycle_t should be int (typically 16-bit)
        static_assert(fl::is_same<cycle_t, int>::value,
                     "cycle_t should be int on AVR");

        // Verify it's the expected size for AVR
        CHECK_EQ(sizeof(cycle_t), sizeof(int));
    }
#else
    SUBCASE("cycle_t is fl::i64 on non-AVR platforms") {
        // On non-AVR platforms, cycle_t should be fl::i64
        static_assert(fl::is_same<cycle_t, fl::i64>::value,
                     "cycle_t should be fl::i64 on non-AVR");

        // Verify it's 64-bit
        CHECK_EQ(sizeof(cycle_t), 8);
    }
#endif

    SUBCASE("cycle_t basic arithmetic") {
        cycle_t a = 100;
        cycle_t b = 50;

        CHECK_EQ(a + b, 150);
        CHECK_EQ(a - b, 50);
        CHECK_EQ(a * 2, 200);
        CHECK_EQ(a / 2, 50);
    }

    SUBCASE("cycle_t can represent fixed-point values") {
        // cycle_t is described as 8.8 fixed point
        // This means 8 bits for integer part, 8 bits for fractional part
        // Value 256 would represent 1.0 in 8.8 fixed point

        cycle_t one_fixed = 256;  // 1.0 in 8.8 fixed point
        cycle_t half_fixed = 128; // 0.5 in 8.8 fixed point

        CHECK_EQ(one_fixed + half_fixed, 384); // 1.5 in 8.8 fixed point
        CHECK_EQ(one_fixed * 2, 512);          // 2.0 in 8.8 fixed point
    }

    SUBCASE("cycle_t comparison operations") {
        cycle_t a = 100;
        cycle_t b = 50;
        cycle_t c = 100;

        CHECK(a > b);
        CHECK(b < a);
        CHECK(a >= c);
        CHECK(a <= c);
        CHECK(a == c);
        CHECK(a != b);
    }

    SUBCASE("cycle_t range and limits") {
#if defined(__AVR__)
        // On AVR, int is typically 16-bit signed
        cycle_t max_val = 32767;
        cycle_t min_val = -32768;
#else
        // On non-AVR, we have full 64-bit range
        cycle_t large_val = 1000000000LL;
        cycle_t small_val = -1000000000LL;

        CHECK(large_val > 0);
        CHECK(small_val < 0);
#endif
    }

    SUBCASE("cycle_t default initialization") {
        cycle_t default_value = cycle_t();
        CHECK_EQ(default_value, 0);
    }

    SUBCASE("cycle_t assignment and copy") {
        cycle_t a = 42;
        cycle_t b = a;
        CHECK_EQ(b, 42);

        cycle_t c;
        c = a;
        CHECK_EQ(c, 42);
    }
}
