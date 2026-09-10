// Test file for fl/stl/charconv.h
//
// This file tests character conversion functions provided by fl/stl/charconv.h,
// which provides utilities similar to C++17 <charconv>.
//
// Functions tested:
// - fl::to_hex(): Convert integer values to hexadecimal string representation

#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/stl/string.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// ============================================================================
// Hexadecimal Conversion Tests
// ============================================================================

FL_TEST_CASE("fl::to_hex - zero value") {
    FL_SUBCASE("zero") {
        // Default behavior is minimal representation
        FL_CHECK_EQ(to_hex(0), "0");
        FL_CHECK_EQ(to_hex(0, false), "0");
        FL_CHECK_EQ(to_hex(0, true), "0");
        // With padding enabled
        FL_CHECK_EQ(to_hex(0, false, true), "00000000");
        FL_CHECK_EQ(to_hex(0, true, true), "00000000");
    }
}

FL_TEST_CASE("fl::to_hex - positive integers") {
    FL_SUBCASE("single digit") {
        // Default behavior is minimal representation
        FL_CHECK_EQ(to_hex(1), "1");
        FL_CHECK_EQ(to_hex(9), "9");
        FL_CHECK_EQ(to_hex(15), "f");
        FL_CHECK_EQ(to_hex(15, true), "F");
        // With padding enabled (int is 32-bit = 8 hex chars)
        FL_CHECK_EQ(to_hex(1, false, true), "00000001");
        FL_CHECK_EQ(to_hex(15, true, true), "0000000F");
    }

    FL_SUBCASE("multiple digits") {
        // Minimal representation
        FL_CHECK_EQ(to_hex(16), "10");
        FL_CHECK_EQ(to_hex(255), "ff");
        FL_CHECK_EQ(to_hex(255, true), "FF");
        FL_CHECK_EQ(to_hex(256), "100");
        FL_CHECK_EQ(to_hex(4095), "fff");
        FL_CHECK_EQ(to_hex(4095, true), "FFF");
        // With padding enabled
        FL_CHECK_EQ(to_hex(255, false, true), "000000ff");
        FL_CHECK_EQ(to_hex(4095, true, true), "00000FFF");
    }

    FL_SUBCASE("large values") {
        FL_CHECK_EQ(to_hex(65535), "ffff");
        FL_CHECK_EQ(to_hex(65535, true), "FFFF");
        FL_CHECK_EQ(to_hex(0xDEADBEEF), "deadbeef");
        FL_CHECK_EQ(to_hex(0xDEADBEEF, true), "DEADBEEF");
        // With padding enabled
        FL_CHECK_EQ(to_hex(65535, false, true), "0000ffff");
        FL_CHECK_EQ(to_hex(0xDEADBEEF, true, true), "DEADBEEF");
    }
}

FL_TEST_CASE("fl::to_hex - negative integers") {
    FL_SUBCASE("negative values") {
        // Negative sign before minimal hex value
        FL_CHECK_EQ(to_hex(-1), "-1");
        FL_CHECK_EQ(to_hex(-16), "-10");
        FL_CHECK_EQ(to_hex(-255), "-ff");
        FL_CHECK_EQ(to_hex(-255, true), "-FF");
        // With padding enabled
        FL_CHECK_EQ(to_hex(-1, false, true), "-00000001");
        FL_CHECK_EQ(to_hex(-255, true, true), "-000000FF");
    }
}

FL_TEST_CASE("fl::to_hex - different integer types") {
    FL_SUBCASE("uint8_t") {
        uint8_t val = 0xAB;
        // Minimal representation
        FL_CHECK_EQ(to_hex(val), "ab");
        FL_CHECK_EQ(to_hex(val, true), "AB");
        // With padding (8-bit = 2 hex chars)
        FL_CHECK_EQ(to_hex(val, false, true), "ab");
        FL_CHECK_EQ(to_hex(val, true, true), "AB");
    }

    FL_SUBCASE("uint16_t") {
        uint16_t val = 0x1234;
        // Minimal representation
        FL_CHECK_EQ(to_hex(val), "1234");
        FL_CHECK_EQ(to_hex(val, true), "1234");
        // With padding (16-bit = 4 hex chars) - already full width
        FL_CHECK_EQ(to_hex(val, false, true), "1234");
        FL_CHECK_EQ(to_hex(val, true, true), "1234");
    }

    FL_SUBCASE("uint32_t") {
        uint32_t val = 0xABCD1234;
        // Minimal representation
        FL_CHECK_EQ(to_hex(val), "abcd1234");
        FL_CHECK_EQ(to_hex(val, true), "ABCD1234");
        // With padding (32-bit = 8 hex chars) - already full width
        FL_CHECK_EQ(to_hex(val, false, true), "abcd1234");
        FL_CHECK_EQ(to_hex(val, true, true), "ABCD1234");
    }

    FL_SUBCASE("int8_t") {
        int8_t val = -16;
        // Minimal representation with negative sign
        FL_CHECK_EQ(to_hex(val), "-10");
        // With padding (8-bit = 2 hex chars)
        FL_CHECK_EQ(to_hex(val, false, true), "-10");
    }

    FL_SUBCASE("int16_t") {
        int16_t val = -256;
        // Minimal representation with negative sign
        FL_CHECK_EQ(to_hex(val), "-100");
        // With padding (16-bit = 4 hex chars)
        FL_CHECK_EQ(to_hex(val, false, true), "-0100");
    }
}

FL_TEST_CASE("fl::to_hex - case sensitivity") {
    FL_SUBCASE("lowercase") {
        // Minimal representation
        FL_CHECK_EQ(to_hex(0xABCDEF, false), "abcdef");
        FL_CHECK_EQ(to_hex(0xFEDCBA, false), "fedcba");
        // With padding (32-bit int = 8 hex chars)
        FL_CHECK_EQ(to_hex(0xABCDEF, false, true), "00abcdef");
        FL_CHECK_EQ(to_hex(0xFEDCBA, false, true), "00fedcba");
    }

    FL_SUBCASE("uppercase") {
        // Minimal representation
        FL_CHECK_EQ(to_hex(0xABCDEF, true), "ABCDEF");
        FL_CHECK_EQ(to_hex(0xFEDCBA, true), "FEDCBA");
        // With padding
        FL_CHECK_EQ(to_hex(0xABCDEF, true, true), "00ABCDEF");
        FL_CHECK_EQ(to_hex(0xFEDCBA, true, true), "00FEDCBA");
    }
}

// ---------------------------------------------------------------------------
// ftoa over the whole float range
//
// Recorded as an aside on #4156, where it nearly caused a live finite check to
// be deleted as dead code: `FL_WARN` printed *every* over-range float as
// `-21474836.48`. The cause is `static_cast<int>` of an out-of-range float,
// which is undefined behaviour and lands on INT_MIN on the common targets.
// The old boundary was about 2.1e7 at the default precision of two -- 21.5
// million, which a microsecond count, a byte total or a raw s16.16 value
// passes without being unusual.
// ---------------------------------------------------------------------------

namespace {

fl::string ftoaOf(float value, int precision) {
    char buf[64] = {0};
    fl::ftoa(value, buf, precision);
    return fl::string(buf);
}

float positiveInfinity() {
    float zero = 0.0f;
    return 1.0f / zero;
}

}  // namespace

FL_TEST_CASE("ftoa - magnitudes past the old int boundary print correctly") {
    // 21 million was fine before, 22 million was not. Both are ordinary.
    FL_CHECK_EQ(ftoaOf(21000000.0f, 2), fl::string("21000000.00"));
    FL_CHECK_EQ(ftoaOf(22000000.0f, 2), fl::string("22000000.00"));
    FL_CHECK_EQ(ftoaOf(-22000000.0f, 2), fl::string("-22000000.00"));
    FL_CHECK_EQ(ftoaOf(1000000000.0f, 2), fl::string("1000000000.00"));
}

FL_TEST_CASE("ftoa - the integer part is not lost to the scaling") {
    // Scaling the whole value by the multiplier in float is what cost the low
    // digits: 1e9 * 100 is 1e11, which no float represents, so this used to
    // read 999999979.52 even once the integer overflow was gone. Only the
    // fractional residue is scaled now.
    FL_CHECK_EQ(ftoaOf(1000000000.0f, 2), fl::string("1000000000.00"));
    FL_CHECK_EQ(ftoaOf(16777216.0f, 3), fl::string("16777216.000"));
}

FL_TEST_CASE("ftoa - the whole 64-bit band renders as digits") {
    // The band between 2^31 and the scientific threshold is where an `int`
    // accumulator is still undefined and an `i64` one is still exact. Powers
    // of two because they are exact in both a float and an i64, so the
    // expected string is not itself an approximation.
    FL_CHECK_EQ(ftoaOf(8589934592.0f, 2), fl::string("8589934592.00"));         // 2^33
    FL_CHECK_EQ(ftoaOf(1125899906842624.0f, 2),
                fl::string("1125899906842624.00"));                            // 2^50
    FL_CHECK_EQ(ftoaOf(-8589934592.0f, 2), fl::string("-8589934592.00"));
    FL_CHECK_EQ(ftoaOf(8589934592.0f, 0), fl::string("8589934592"));
}

FL_TEST_CASE("ftoa - a magnitude beyond any integer goes to scientific") {
    // Not representable digit by digit at all, so it is printed in a shape the
    // reader has to decode rather than as a wrong decimal.
    FL_CHECK_EQ(ftoaOf(1e30f, 2), fl::string("1.00e+30"));
    FL_CHECK_EQ(ftoaOf(-1e30f, 2), fl::string("-1.00e+30"));
    FL_CHECK_EQ(ftoaOf(3.0e38f, 2), fl::string("3.00e+38"));
    // Two-digit exponents are zero padded so the width is stable in a log.
    FL_CHECK_EQ(ftoaOf(5e19f, 1), fl::string("5.0e+19"));
}

FL_TEST_CASE("ftoa - non-finite values say what they are") {
    const float inf = positiveInfinity();
    FL_CHECK_EQ(ftoaOf(inf, 2), fl::string("inf"));
    FL_CHECK_EQ(ftoaOf(-inf, 2), fl::string("-inf"));
    FL_CHECK_EQ(ftoaOf(inf - inf, 2), fl::string("nan"));
    // And at every precision, including the two special paths.
    FL_CHECK_EQ(ftoaOf(inf, 0), fl::string("inf"));
    FL_CHECK_EQ(ftoaOf(inf, -1), fl::string("inf"));
}

FL_TEST_CASE("ftoa - the ordinary cases are untouched") {
    // The regression guard. Everything above is about the tail of the range;
    // none of it may move what the common path prints.
    FL_CHECK_EQ(ftoaOf(0.0f, 2), fl::string("0.00"));
    FL_CHECK_EQ(ftoaOf(1.5f, 2), fl::string("1.50"));
    FL_CHECK_EQ(ftoaOf(-1.5f, 2), fl::string("-1.50"));
    FL_CHECK_EQ(ftoaOf(1.0f, 2), fl::string("1.00"));
    FL_CHECK_EQ(ftoaOf(0.125f, 3), fl::string("0.125"));
    FL_CHECK_EQ(ftoaOf(-0.5f, 1), fl::string("-0.5"));
    FL_CHECK_EQ(ftoaOf(2.5f, 0), fl::string("3"));
    FL_CHECK_EQ(ftoaOf(-2.5f, 0), fl::string("-3"));
    FL_CHECK_EQ(ftoaOf(0.0f, 0), fl::string("0"));
}

} // FL_TEST_FILE
