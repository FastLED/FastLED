// Unit tests for fl::ieee754_string -- integer-only IEEE 754 decimal codec.
//
// Coverage focus: the parser path (task #1 of FastLED #3022 phase 2). The
// serializer path is intentionally left for the follow-up task -- its tests
// will land alongside its implementation.

#include "test.h"

#include "fl/stl/ieee754_string.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/string.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Convenience: parse a literal and reinterpret the result as a float for
// comparison against host strtof. The bit-cast itself does not anchor any
// libgcc soft-FP helper; the host `float` comparison does (cheap on host).
static float parse_as_float(const char* s) {
    u32 bits = ieee754_parse_decimal(s, fl::strlen(s));
    return fl::bit_cast<float>(bits);
}

FL_TEST_CASE("ieee754_parse_decimal -- well-known constants") {
    FL_SUBCASE("zero") {
        FL_CHECK(ieee754_parse_decimal("0", 1) == 0x00000000u);
        FL_CHECK(ieee754_parse_decimal("-0", 2) == 0x80000000u);
        FL_CHECK(ieee754_parse_decimal("0.0", 3) == 0x00000000u);
    }

    FL_SUBCASE("one") {
        FL_CHECK(ieee754_parse_decimal("1", 1) == 0x3F800000u);
        FL_CHECK(ieee754_parse_decimal("1.0", 3) == 0x3F800000u);
        FL_CHECK(ieee754_parse_decimal("-1", 2) == 0xBF800000u);
    }

    FL_SUBCASE("ten") {
        FL_CHECK(ieee754_parse_decimal("10", 2) == 0x41200000u);
        FL_CHECK(ieee754_parse_decimal("10.0", 4) == 0x41200000u);
    }

    FL_SUBCASE("half") {
        FL_CHECK(ieee754_parse_decimal("0.5", 3) == 0x3F000000u);
        FL_CHECK(ieee754_parse_decimal("-0.5", 4) == 0xBF000000u);
    }

    FL_SUBCASE("two") {
        FL_CHECK(ieee754_parse_decimal("2", 1) == 0x40000000u);
        FL_CHECK(ieee754_parse_decimal("2.0", 3) == 0x40000000u);
    }
}

FL_TEST_CASE("ieee754_parse_decimal -- within +/-1 ULP of strtof") {
    auto ulp_diff = [](u32 a, u32 b) -> u32 {
        return a > b ? (a - b) : (b - a);
    };

    auto roundtrip_check = [&](const char* s, float expected) {
        u32 got_bits = ieee754_parse_decimal(s, fl::strlen(s));
        u32 expected_bits = fl::bit_cast<u32>(expected);
        FL_CHECK(ulp_diff(got_bits, expected_bits) <= 1u);
    };

    roundtrip_check("3.14", 3.14f);
    roundtrip_check("3.14159", 3.14159f);
    roundtrip_check("0.001", 0.001f);
    roundtrip_check("0.0001", 0.0001f);
    roundtrip_check("100.0", 100.0f);
    roundtrip_check("1000.0", 1000.0f);
    roundtrip_check("12345.6789", 12345.6789f);
}

FL_TEST_CASE("ieee754_parse_decimal -- scientific notation") {
    auto bits_of = [](float f) { return fl::bit_cast<u32>(f); };

    FL_CHECK(ieee754_parse_decimal("1e0", 3) == bits_of(1.0f));
    FL_CHECK(ieee754_parse_decimal("1e1", 3) == bits_of(10.0f));
    FL_CHECK(ieee754_parse_decimal("1e-1", 4) == bits_of(0.1f));
    FL_CHECK(ieee754_parse_decimal("2.5e3", 5) == bits_of(2500.0f));
    FL_CHECK(ieee754_parse_decimal("-1e2", 4) == bits_of(-100.0f));
}

FL_TEST_CASE("ieee754_parse_decimal -- overflow / underflow clamp") {
    // 10**40 > single-precision max -- clamp to +inf.
    FL_CHECK(ieee754_parse_decimal("1e40", 4) == 0x7F800000u);
    FL_CHECK(ieee754_parse_decimal("-1e40", 5) == 0xFF800000u);

    // 10**-50 < single-precision smallest denormal -- clamp to +/-0.
    FL_CHECK(ieee754_parse_decimal("1e-50", 5) == 0x00000000u);
    FL_CHECK(ieee754_parse_decimal("-1e-50", 6) == 0x80000000u);
}

FL_TEST_CASE("ieee754_parse_decimal -- consumed bookkeeping") {
    fl::size n = 0;
    u32 bits = ieee754_parse_decimal("3.14xyz", 7, &n);
    FL_CHECK(n == 4);  // "3.14"
    FL_CHECK(bits != 0u);

    // Empty string returns 0 and zero consumed.
    bits = ieee754_parse_decimal("", 0, &n);
    FL_CHECK(n == 0);
    FL_CHECK(bits == 0u);

    // Lone sign with no digits -- invalid, zero consumed.
    bits = ieee754_parse_decimal("-", 1, &n);
    FL_CHECK(n == 0);
    FL_CHECK(bits == 0u);
}

FL_TEST_CASE("ieee754_parse_decimal -- integration via bit_cast<float>") {
    // Make sure the parser composes cleanly with bit_cast<float>() -- the
    // intended API integration pattern for the JSON tokenizer.
    float a = parse_as_float("3.14");
    FL_CHECK(a > 3.13f);
    FL_CHECK(a < 3.15f);

    float b = parse_as_float("-2.5");
    FL_CHECK(b < -2.4f);
    FL_CHECK(b > -2.6f);
}

FL_TEST_CASE("ieee754_format_decimal -- well-known constants") {
    FL_SUBCASE("zero") {
        FL_CHECK(ieee754_format_decimal(0x00000000u, 6) == fl::string("0.000000"));
        FL_CHECK(ieee754_format_decimal(0x80000000u, 6) == fl::string("-0.000000"));
        FL_CHECK(ieee754_format_decimal(0x00000000u, 0) == fl::string("0"));
    }

    FL_SUBCASE("one") {
        FL_CHECK(ieee754_format_decimal(0x3F800000u, 6) == fl::string("1.000000"));
        FL_CHECK(ieee754_format_decimal(0xBF800000u, 6) == fl::string("-1.000000"));
        FL_CHECK(ieee754_format_decimal(0x3F800000u, 0) == fl::string("1"));
    }

    FL_SUBCASE("half") {
        FL_CHECK(ieee754_format_decimal(0x3F000000u, 6) == fl::string("0.500000"));
        FL_CHECK(ieee754_format_decimal(0xBF000000u, 6) == fl::string("-0.500000"));
    }

    FL_SUBCASE("ten and hundred") {
        FL_CHECK(ieee754_format_decimal(0x41200000u, 2) == fl::string("10.00"));
        FL_CHECK(ieee754_format_decimal(0x42C80000u, 1) == fl::string("100.0"));
    }
}

FL_TEST_CASE("ieee754_format_decimal -- special values") {
    FL_CHECK(ieee754_format_decimal(0x7F800000u, 6) == fl::string("inf"));
    FL_CHECK(ieee754_format_decimal(0xFF800000u, 6) == fl::string("-inf"));
    FL_CHECK(ieee754_format_decimal(0x7FC00000u, 6) == fl::string("nan"));
}

FL_TEST_CASE("ieee754_format_decimal -- round-trip via parser") {
    // The serializer + parser should round-trip simple decimal values to
    // within +/-1 ULP. Pick values whose decimal representation terminates so
    // we can compare bit patterns directly.
    auto round_trip = [](u32 bits) -> u32 {
        fl::string text = ieee754_format_decimal(bits, 9);
        return ieee754_parse_decimal(text.c_str(), text.size());
    };

    auto ulp_diff = [](u32 a, u32 b) -> u32 {
        return a > b ? (a - b) : (b - a);
    };

    // Powers of two divide cleanly in binary.
    FL_CHECK(round_trip(0x3F800000u) == 0x3F800000u);  // 1.0
    FL_CHECK(round_trip(0x40000000u) == 0x40000000u);  // 2.0
    FL_CHECK(round_trip(0x3F000000u) == 0x3F000000u);  // 0.5

    // Decimal values may drift by 1 ULP -- that's the embedded-grade target.
    FL_CHECK(ulp_diff(round_trip(0x40490FDBu), 0x40490FDBu) <= 1u);  // pi-ish
    FL_CHECK(ulp_diff(round_trip(0x402DF854u), 0x402DF854u) <= 1u);  // e-ish

    // Negative values keep the sign.
    FL_CHECK(round_trip(0xBF800000u) == 0xBF800000u);  // -1.0
    FL_CHECK(round_trip(0xC1200000u) == 0xC1200000u);  // -10.0
}

FL_TEST_CASE("ieee754_format_decimal -- precision clamping") {
    FL_CHECK(ieee754_format_decimal(0x3F800000u, -1) == fl::string("1"));
    FL_CHECK(ieee754_format_decimal(0x3F800000u, 15) == fl::string("1.000000000"));
}

}
