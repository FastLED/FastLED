// u12x4 and u16x16 unsigned fixed-point type tests
// Extracted from fixed_point.cpp (sub-issue of #3127).

#include "test.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point.h"
#include "fl/math/math.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace { // Anonymous namespace for u12x4+u16x16 tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

// u12x4 tests (from u12x4.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u12x4 - Construction") {
    FL_SUBCASE("Default constructor") {
        u12x4 a;
        FL_CHECK(a.raw() == 0);
        FL_CHECK(a.to_int() == 0);
        FL_CHECK(a.to_float() == 0.0f);
    }

    FL_SUBCASE("Float constructor") {
        u12x4 a(1.0f);
        FL_CHECK(a.raw() == 16);  // 1.0 * 2^4 = 16
        FL_CHECK(a.to_int() == 1);
        FL_CHECK(a.to_float() == 1.0f);

        u12x4 b(2.5f);
        FL_CHECK(b.raw() == 40);  // 2.5 * 2^4 = 40
        FL_CHECK(b.to_int() == 2);
        FL_CHECK(b.to_float() == 2.5f);

        u12x4 c(0.5f);
        FL_CHECK(c.raw() == 8);   // 0.5 * 2^4 = 8
        FL_CHECK(c.to_int() == 0);
        FL_CHECK(c.to_float() == 0.5f);
    }

    FL_SUBCASE("from_raw constructor") {
        u12x4 a = u12x4::from_raw(16);
        FL_CHECK(a.to_float() == 1.0f);

        u12x4 b = u12x4::from_raw(40);
        FL_CHECK(b.to_float() == 2.5f);

        u12x4 c = u12x4::from_raw(0);
        FL_CHECK(c.to_float() == 0.0f);
    }

    FL_SUBCASE("Large values") {
        u12x4 a(100.0f);
        FL_CHECK(a.to_int() == 100);
        FL_CHECK(a.to_float() == 100.0f);

        u12x4 b(4095.0f);  // Max 12-bit value
        FL_CHECK(b.to_int() == 4095);
        FL_CHECK(b.to_float() == 4095.0f);
    }
}

FL_TEST_CASE("u12x4 - Addition") {
    FL_SUBCASE("Basic addition") {
        u12x4 a(1.0f);
        u12x4 b(2.0f);
        u12x4 c = a + b;
        FL_CHECK(c.to_float() == 3.0f);
    }

    FL_SUBCASE("Fractional addition") {
        u12x4 a(1.5f);
        u12x4 b(2.5f);
        u12x4 c = a + b;
        FL_CHECK(c.to_float() == 4.0f);
    }

    FL_SUBCASE("Addition with zero") {
        u12x4 a(5.0f);
        u12x4 b(0.0f);
        u12x4 c = a + b;
        FL_CHECK(c.to_float() == 5.0f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(1.0f);
        a += u12x4(2.0f);
        FL_CHECK(a.to_float() == 3.0f);
    }
}

FL_TEST_CASE("u12x4 - Subtraction") {
    FL_SUBCASE("Basic subtraction") {
        u12x4 a(5.0f);
        u12x4 b(2.0f);
        u12x4 c = a - b;
        FL_CHECK(c.to_float() == 3.0f);
    }

    FL_SUBCASE("Fractional subtraction") {
        u12x4 a(3.5f);
        u12x4 b(1.5f);
        u12x4 c = a - b;
        FL_CHECK(c.to_float() == 2.0f);
    }

    FL_SUBCASE("Subtraction to zero") {
        u12x4 a(5.0f);
        u12x4 b(5.0f);
        u12x4 c = a - b;
        FL_CHECK(c.to_float() == 0.0f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(5.0f);
        a -= u12x4(2.0f);
        FL_CHECK(a.to_float() == 3.0f);
    }
}

FL_TEST_CASE("u12x4 - Multiplication") {
    FL_SUBCASE("Basic multiplication") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = a * b;
        FL_CHECK(c.to_float() == 6.0f);
    }

    FL_SUBCASE("Fractional multiplication") {
        u12x4 a(1.5f);
        u12x4 b(2.0f);
        u12x4 c = a * b;
        FL_CHECK(c.to_float() == 3.0f);

        u12x4 d(0.5f);
        u12x4 e(0.5f);
        u12x4 f = d * e;
        FL_CHECK(f.to_float() == doctest::Approx(0.25f).epsilon(0.1f));
    }

    FL_SUBCASE("Multiplication by zero") {
        u12x4 a(5.0f);
        u12x4 b(0.0f);
        u12x4 c = a * b;
        FL_CHECK(c.to_float() == 0.0f);
    }

    FL_SUBCASE("Scalar multiplication") {
        u12x4 a(2.5f);
        u12x4 b = a * 2;
        FL_CHECK(b.to_float() == 5.0f);

        u12x4 c = 3 * a;
        FL_CHECK(c.to_float() == 7.5f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(2.0f);
        a *= u12x4(3.0f);
        FL_CHECK(a.to_float() == 6.0f);
    }
}

FL_TEST_CASE("u12x4 - Division") {
    FL_SUBCASE("Basic division") {
        u12x4 a(6.0f);
        u12x4 b(2.0f);
        u12x4 c = a / b;
        FL_CHECK(c.to_float() == 3.0f);
    }

    FL_SUBCASE("Fractional division") {
        u12x4 a(5.0f);
        u12x4 b(2.0f);
        u12x4 c = a / b;
        FL_CHECK(c.to_float() == doctest::Approx(2.5f).epsilon(0.1f));

        u12x4 d(1.0f);
        u12x4 e(4.0f);
        u12x4 f = d / e;
        FL_CHECK(f.to_float() == doctest::Approx(0.25f).epsilon(0.1f));
    }

    FL_SUBCASE("Division by one") {
        u12x4 a(5.0f);
        u12x4 b(1.0f);
        u12x4 c = a / b;
        FL_CHECK(c.to_float() == 5.0f);
    }

    FL_SUBCASE("Compound assignment") {
        u12x4 a(6.0f);
        a /= u12x4(2.0f);
        FL_CHECK(a.to_float() == 3.0f);
    }
}

FL_TEST_CASE("u12x4 - Shifts") {
    FL_SUBCASE("Right shift") {
        u12x4 a(8.0f);
        u12x4 b = a >> 1;  // Shift raw value right by 1
        FL_CHECK(b.to_float() == 4.0f);

        u12x4 c = a >> 2;
        FL_CHECK(c.to_float() == 2.0f);
    }

    FL_SUBCASE("Left shift") {
        u12x4 a(2.0f);
        u12x4 b = a << 1;  // Shift raw value left by 1
        FL_CHECK(b.to_float() == 4.0f);

        u12x4 c = a << 2;
        FL_CHECK(c.to_float() == 8.0f);
    }
}

FL_TEST_CASE("u12x4 - Comparisons") {
    FL_SUBCASE("Less than") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        FL_CHECK(a < b);
        FL_CHECK_FALSE(b < a);
        FL_CHECK_FALSE(a < a);
    }

    FL_SUBCASE("Greater than") {
        u12x4 a(3.0f);
        u12x4 b(2.0f);
        FL_CHECK(a > b);
        FL_CHECK_FALSE(b > a);
        FL_CHECK_FALSE(a > a);
    }

    FL_SUBCASE("Less than or equal") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        FL_CHECK(a <= b);
        FL_CHECK(a <= a);
        FL_CHECK_FALSE(b <= a);
    }

    FL_SUBCASE("Greater than or equal") {
        u12x4 a(3.0f);
        u12x4 b(2.0f);
        FL_CHECK(a >= b);
        FL_CHECK(a >= a);
        FL_CHECK_FALSE(b >= a);
    }

    FL_SUBCASE("Equality") {
        u12x4 a(2.5f);
        u12x4 b(2.5f);
        u12x4 c(3.0f);
        FL_CHECK(a == b);
        FL_CHECK_FALSE(a == c);
    }

    FL_SUBCASE("Inequality") {
        u12x4 a(2.5f);
        u12x4 b(3.0f);
        FL_CHECK(a != b);
        FL_CHECK_FALSE(a != a);
    }
}

FL_TEST_CASE("u12x4 - Math functions") {
    FL_SUBCASE("mod") {
        u12x4 a(7.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::mod(a, b);
        FL_CHECK(c.to_float() == doctest::Approx(1.0f).epsilon(0.1f));
    }

    FL_SUBCASE("floor") {
        u12x4 a(2.75f);
        u12x4 b = u12x4::floor(a);
        FL_CHECK(b.to_float() == 2.0f);

        u12x4 c(5.0f);
        u12x4 d = u12x4::floor(c);
        FL_CHECK(d.to_float() == 5.0f);
    }

    FL_SUBCASE("ceil") {
        u12x4 a(2.25f);
        u12x4 b = u12x4::ceil(a);
        FL_CHECK(b.to_float() == 3.0f);

        u12x4 c(5.0f);
        u12x4 d = u12x4::ceil(c);
        FL_CHECK(d.to_float() == 5.0f);
    }

    FL_SUBCASE("fract") {
        u12x4 a(2.75f);
        u12x4 b = u12x4::fract(a);
        FL_CHECK(b.to_float() == doctest::Approx(0.75f).epsilon(0.1f));

        u12x4 c(5.0f);
        u12x4 d = u12x4::fract(c);
        FL_CHECK(d.to_float() == 0.0f);
    }

    FL_SUBCASE("abs") {
        // For unsigned, abs is identity
        u12x4 a(5.5f);
        u12x4 b = u12x4::abs(a);
        FL_CHECK(b.to_float() == 5.5f);

        u12x4 c(0.0f);
        u12x4 d = u12x4::abs(c);
        FL_CHECK(d.to_float() == 0.0f);
    }

    FL_SUBCASE("min") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::min(a, b);
        FL_CHECK(c.to_float() == 2.0f);

        u12x4 d = u12x4::min(b, a);
        FL_CHECK(d.to_float() == 2.0f);
    }

    FL_SUBCASE("max") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::max(a, b);
        FL_CHECK(c.to_float() == 3.0f);

        u12x4 d = u12x4::max(b, a);
        FL_CHECK(d.to_float() == 3.0f);
    }

    FL_SUBCASE("clamp") {
        u12x4 lo(1.0f);
        u12x4 hi(5.0f);

        u12x4 a(3.0f);
        u12x4 b = u12x4::clamp(a, lo, hi);
        FL_CHECK(b.to_float() == 3.0f);

        u12x4 c(0.5f);
        u12x4 d = u12x4::clamp(c, lo, hi);
        FL_CHECK(d.to_float() == 1.0f);

        u12x4 e(6.0f);
        u12x4 f = u12x4::clamp(e, lo, hi);
        FL_CHECK(f.to_float() == 5.0f);
    }

    FL_SUBCASE("lerp") {
        u12x4 a(0.0f);
        u12x4 b(10.0f);

        u12x4 c = u12x4::lerp(a, b, u12x4(0.0f));
        FL_CHECK(c.to_float() == doctest::Approx(0.0f).epsilon(0.1f));

        u12x4 d = u12x4::lerp(a, b, u12x4(0.5f));
        FL_CHECK(d.to_float() == doctest::Approx(5.0f).epsilon(0.2f));

        u12x4 e = u12x4::lerp(a, b, u12x4(1.0f));
        FL_CHECK(e.to_float() == doctest::Approx(10.0f).epsilon(0.2f));
    }

    FL_SUBCASE("step") {
        u12x4 edge(5.0f);

        u12x4 a = u12x4::step(edge, u12x4(3.0f));
        FL_CHECK(a.to_float() == 0.0f);

        u12x4 b = u12x4::step(edge, u12x4(5.0f));
        FL_CHECK(b.to_float() == 1.0f);  // x >= edge returns 1

        u12x4 c = u12x4::step(edge, u12x4(7.0f));
        FL_CHECK(c.to_float() == 1.0f);
    }

    FL_SUBCASE("smoothstep") {
        u12x4 edge0(0.0f);
        u12x4 edge1(1.0f);

        u12x4 a = u12x4::smoothstep(edge0, edge1, u12x4(0.0f));
        FL_CHECK(a.to_float() == doctest::Approx(0.0f).epsilon(0.1f));

        u12x4 b = u12x4::smoothstep(edge0, edge1, u12x4(0.5f));
        FL_CHECK(b.to_float() == doctest::Approx(0.5f).epsilon(0.15f));

        u12x4 c = u12x4::smoothstep(edge0, edge1, u12x4(1.0f));
        FL_CHECK(c.to_float() == doctest::Approx(1.0f).epsilon(0.1f));
    }
}

FL_TEST_CASE("u12x4 - Advanced math") {
    FL_SUBCASE("sqrt") {
        u12x4 a(4.0f);
        u12x4 b = u12x4::sqrt(a);
        FL_CHECK(b.to_float() == doctest::Approx(2.0f).epsilon(0.1f));

        u12x4 c(9.0f);
        u12x4 d = u12x4::sqrt(c);
        FL_CHECK(d.to_float() == doctest::Approx(3.0f).epsilon(0.1f));

        u12x4 e(0.0f);
        u12x4 f = u12x4::sqrt(e);
        FL_CHECK(f.to_float() == 0.0f);
    }

    FL_SUBCASE("rsqrt") {
        u12x4 a(4.0f);
        u12x4 b = u12x4::rsqrt(a);
        FL_CHECK(b.to_float() == doctest::Approx(0.5f).epsilon(0.1f));

        u12x4 c(9.0f);
        u12x4 d = u12x4::rsqrt(c);
        FL_CHECK(d.to_float() == doctest::Approx(0.333f).epsilon(0.1f));
    }

    FL_SUBCASE("pow") {
        u12x4 a(2.0f);
        u12x4 b(3.0f);
        u12x4 c = u12x4::pow(a, b);
        FL_CHECK(c.to_float() == doctest::Approx(8.0f).epsilon(0.5f));

        u12x4 d(5.0f);
        u12x4 e(2.0f);
        u12x4 f = u12x4::pow(d, e);
        FL_CHECK(f.to_float() == doctest::Approx(25.0f).epsilon(1.0f));

        // x^0 = 1
        u12x4 g(10.0f);
        u12x4 h(0.0f);
        u12x4 i = u12x4::pow(g, h);
        FL_CHECK(i.to_float() == doctest::Approx(1.0f).epsilon(0.1f));

        // 1^x = 1
        u12x4 j(1.0f);
        u12x4 k(5.0f);
        u12x4 l = u12x4::pow(j, k);
        FL_CHECK(l.to_float() == doctest::Approx(1.0f).epsilon(0.1f));
    }
}

FL_TEST_CASE("u12x4 - Edge cases") {
    FL_SUBCASE("Zero value") {
        u12x4 zero(0.0f);
        FL_CHECK(zero.raw() == 0);
        FL_CHECK(zero.to_int() == 0);
        FL_CHECK(zero.to_float() == 0.0f);
    }

    FL_SUBCASE("Maximum value") {
        // Max value with 12 integer bits is 4095.9375 (0xFFFF / 16)
        u12x4 max = u12x4::from_raw(0xFFFF);
        FL_CHECK(max.to_int() == 4095);
        FL_CHECK(max.to_float() == doctest::Approx(4095.9375f).epsilon(0.01f));
    }

    FL_SUBCASE("Small fractional values") {
        // Smallest representable non-zero value is 1/16 = 0.0625
        u12x4 small = u12x4::from_raw(1);
        FL_CHECK(small.to_float() == doctest::Approx(0.0625f).epsilon(0.001f));
    }

    FL_SUBCASE("Operations at boundaries") {
        u12x4 zero(0.0f);
        u12x4 one(1.0f);

        // 0 * anything = 0
        u12x4 a = zero * u12x4(100.0f);
        FL_CHECK(a.to_float() == 0.0f);

        // 1 * anything = anything
        u12x4 b = one * u12x4(5.5f);
        FL_CHECK(b.to_float() == doctest::Approx(5.5f).epsilon(0.1f));

        // anything + 0 = anything
        u12x4 c = u12x4(7.5f) + zero;
        FL_CHECK(c.to_float() == doctest::Approx(7.5f).epsilon(0.1f));
    }
}

FL_TEST_CASE("u12x4 - Precision") {
    FL_SUBCASE("4-bit fractional precision") {
        // With 4 fractional bits, precision is 1/16 = 0.0625
        float values[] = {0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f};
        for (float val : values) {
            u12x4 a(val);
            FL_CHECK(a.to_float() == doctest::Approx(val).epsilon(0.001f));
        }
    }

    FL_SUBCASE("Rounding behavior") {
        // Values between representable steps should round down due to truncation
        u12x4 a(1.03f);  // Should become 1.0 (16/16)
        FL_CHECK(a.to_float() == doctest::Approx(1.0f).epsilon(0.1f));

        u12x4 b(1.07f);  // Should become 1.0625 (17/16)
        FL_CHECK(b.to_float() == doctest::Approx(1.0625f).epsilon(0.1f));
    }
}


// ---------------------------------------------------------------------------
// u16x16 tests (from u16x16.cpp)
// ---------------------------------------------------------------------------

constexpr float tol() { return 0.0005f; }
constexpr float sqrt_tol() { return 0.0002f; }
constexpr float pow_tol() { return 0.015f; }
constexpr float smooth_tol() { return 0.0005f; }
constexpr float rt_tol() { return 0.001f; }

void check_near(u16x16 val, float expected, float tolerance) {
    FL_CHECK_CLOSE(val.to_float(), expected, tolerance);
}

void check_near(u16x16 val, float expected) {
    check_near(val, expected, tol());
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - default construction") {
    u16x16 a;
    FL_CHECK_EQ(a.raw(), u32(0));
    FL_CHECK_EQ(a.to_int(), u32(0));
}

FL_TEST_CASE("u16x16 - float construction") {
    u16x16 one(1.0f);
    FL_CHECK_EQ(one.raw(), u32(1) << u16x16::FRAC_BITS);
    FL_CHECK_EQ(one.to_int(), u32(1));

    u16x16 half(0.5f);
    FL_CHECK_EQ(half.raw(), u32(1) << (u16x16::FRAC_BITS - 1));
    FL_CHECK_EQ(half.to_int(), u32(0));

    u16x16 val(3.0f);
    FL_CHECK_EQ(val.to_int(), u32(3));

    u16x16 zero(0.0f);
    FL_CHECK_EQ(zero.raw(), u32(0));
    FL_CHECK_EQ(zero.to_int(), u32(0));
}

FL_TEST_CASE("u16x16 - from_raw") {
    // 1.0
    u16x16 a = u16x16::from_raw(u32(1) << u16x16::FRAC_BITS);
    FL_CHECK_EQ(a.to_int(), u32(1));

    // 2.5
    u32 raw_2_5 = (u32(2) << u16x16::FRAC_BITS) + (u32(1) << (u16x16::FRAC_BITS - 1));
    u16x16 b = u16x16::from_raw(raw_2_5);
    FL_CHECK_EQ(b.to_int(), u32(2));
    check_near(b, 2.5f);

    // smallest fraction
    u16x16 c = u16x16::from_raw(u32(1));
    FL_CHECK_EQ(c.raw(), u32(1));
    FL_CHECK_EQ(c.to_int(), u32(0));
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - addition") {
    u16x16 a(1.0f), b(2.0f);
    u16x16 c = a + b;
    FL_CHECK_EQ(c.to_int(), u32(3));
    check_near(c, 3.0f);

    // Fractional
    u16x16 d(0.25f), e(0.75f);
    check_near(d + e, 1.0f);

    // Zero identity
    u16x16 zero;
    FL_CHECK_EQ((a + zero).raw(), a.raw());
}

FL_TEST_CASE("u16x16 - subtraction") {
    u16x16 a(3.0f), b(1.0f);
    check_near(a - b, 2.0f);

    // Self subtraction
    u16x16 zero;
    FL_CHECK_EQ((a - a).raw(), zero.raw());

    // Fractional
    u16x16 c(1.75f), d(0.25f);
    check_near(c - d, 1.5f);

    // Underflow wraps (unsigned)
    u16x16 small(0.5f), big(1.0f);
    // big - small = 0.5 (positive result)
    check_near(big - small, 0.5f);
}

FL_TEST_CASE("u16x16 - fixed-point multiply") {
    u16x16 a(2.0f), b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    u16x16 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 = identity
    u16x16 one(1.0f);
    FL_CHECK_EQ((a * one).raw(), a.raw());

    // Multiply by 0 = zero
    u16x16 zero;
    FL_CHECK_EQ((a * zero).raw(), u32(0));

    // Fractional precision
    u16x16 c(1.5f), d(2.0f);
    check_near(c * d, 3.0f);

    // Higher fractional precision
    u16x16 e(1.5f), f(2.5f);
    check_near(e * f, 3.75f);
}

FL_TEST_CASE("u16x16 - fixed-point divide") {
    u16x16 a(6.0f), b(3.0f);
    check_near(a / b, 2.0f);

    u16x16 one(1.0f), two(2.0f);
    check_near(one / two, 0.5f);

    u16x16 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 = identity
    FL_CHECK_EQ((a / one).raw(), a.raw());

    // Fractional result: 1/3
    u16x16 three(3.0f);
    check_near(one / three, 0.3333f, tol() * 10.0f);
}

FL_TEST_CASE("u16x16 - scalar multiply") {
    u16x16 a(1.5f);

    // fp * scalar
    check_near(a * u32(2), 3.0f);
    check_near(a * u32(0), 0.0f);

    // scalar * fp (commutative friend)
    check_near(u32(2) * a, 3.0f);

    // Large scalar multiply
    check_near(a * u32(100), 150.0f, tol() * 10.0f);
}

FL_TEST_CASE("u16x16 - right shift") {
    u16x16 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    u16x16 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);
}

FL_TEST_CASE("u16x16 - left shift") {
    u16x16 a(1.0f);
    check_near(a << 1, 2.0f);
    check_near(a << 2, 4.0f);

    // Fractional shift
    u16x16 b(0.25f);
    check_near(b << 1, 0.5f);
    check_near(b << 2, 1.0f);
}

// ---------------------------------------------------------------------------
// Comparisons
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - comparisons") {
    u16x16 a(1.0f), b(2.0f), c(1.0f);
    u16x16 zero;

    FL_CHECK(a == c);
    FL_CHECK(a != b);
    FL_CHECK_FALSE(a == b);
    FL_CHECK_FALSE(a != c);

    FL_CHECK(a < b);
    FL_CHECK(b > a);
    FL_CHECK_FALSE(b < a);
    FL_CHECK_FALSE(a > b);

    FL_CHECK(a <= b);
    FL_CHECK(a <= c);
    FL_CHECK(b >= a);
    FL_CHECK(a >= c);

    FL_CHECK(zero < a);
    FL_CHECK(a > zero);
}

// ---------------------------------------------------------------------------
// Mixed arithmetic
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - mixed arithmetic expressions") {
    u16x16 a(2.0f), b(3.0f), c(0.5f);
    check_near((a + b) * c, 2.5f);

    u16x16 d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // Round-trip: (a / b) * b ~ a
    check_near((a / b) * b, 2.0f, rt_tol());

    // Chained
    u16x16 one(1.0f);
    u16x16 result = (a + b) * c / one;
    check_near(result, 2.5f);
}

// ---------------------------------------------------------------------------
// to_float
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - to_float") {
    u16x16 one(1.0f);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, tol());

    u16x16 half(0.5f);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, tol());

    u16x16 val(2.5f);
    FL_CHECK_CLOSE(val.to_float(), 2.5f, tol());

    u16x16 zero;
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, tol());
}

// ---------------------------------------------------------------------------
// Math functions
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - sqrt") {
    check_near(u16x16::sqrt(u16x16(4.0f)), 2.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(1.0f)), 1.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(2.0f)), 1.4142f, sqrt_tol());

    FL_CHECK_EQ(u16x16::sqrt(u16x16()).raw(), u32(0)); // sqrt(0) = 0

    check_near(u16x16::sqrt(u16x16(9.0f)), 3.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(16.0f)), 4.0f, sqrt_tol());
    check_near(u16x16::sqrt(u16x16(100.0f)), 10.0f, sqrt_tol());
}

FL_TEST_CASE("u16x16 - rsqrt") {
    check_near(u16x16::rsqrt(u16x16(4.0f)), 0.5f, sqrt_tol());
    check_near(u16x16::rsqrt(u16x16(1.0f)), 1.0f, sqrt_tol());

    FL_CHECK_EQ(u16x16::rsqrt(u16x16()).raw(), u32(0)); // rsqrt(0) = 0
}

FL_TEST_CASE("u16x16 - pow") {
    // 2^2 = 4
    check_near(u16x16::pow(u16x16(2.0f), u16x16(2.0f)), 4.0f, pow_tol());

    // 4^0.5 = 2 (sqrt via pow)
    check_near(u16x16::pow(u16x16(4.0f), u16x16(0.5f)), 2.0f, pow_tol());

    // x^0 = 1
    check_near(u16x16::pow(u16x16(5.0f), u16x16(0.0f)), 1.0f, pow_tol());

    // 0^x = 0
    FL_CHECK_EQ(u16x16::pow(u16x16(), u16x16(2.0f)).raw(), u32(0));

    // 2^3 = 8
    check_near(u16x16::pow(u16x16(2.0f), u16x16(3.0f)), 8.0f, pow_tol());
}

FL_TEST_CASE("u16x16 - sqrt identity") {
    float values[] = {1.0f, 2.0f, 4.0f, 9.0f, 16.0f, 25.0f};
    for (float v : values) {
        u16x16 x(v);
        u16x16 s = u16x16::sqrt(x);
        u16x16 sq = s * s;
        check_near(sq, v, v * 0.005f + 0.005f);
    }
}

FL_TEST_CASE("u16x16 - floor and ceil") {
    check_near(u16x16::floor(u16x16(2.75f)), 2.0f);
    check_near(u16x16::ceil(u16x16(2.75f)), 3.0f);
    check_near(u16x16::floor(u16x16(3.0f)), 3.0f);
    check_near(u16x16::ceil(u16x16(3.0f)), 3.0f);
    check_near(u16x16::floor(u16x16(0.25f)), 0.0f);
    check_near(u16x16::ceil(u16x16(0.25f)), 1.0f);
}

FL_TEST_CASE("u16x16 - fract") {
    check_near(u16x16::fract(u16x16(2.75f)), 0.75f);
    FL_CHECK_EQ(u16x16::fract(u16x16(1.0f)).raw(), u32(0));
    check_near(u16x16::fract(u16x16(0.5f)), 0.5f);
}

FL_TEST_CASE("u16x16 - min and max") {
    check_near(u16x16::min(u16x16(3.5f), u16x16(2.0f)), 2.0f);
    check_near(u16x16::max(u16x16(3.5f), u16x16(2.0f)), 3.5f);
    check_near(u16x16::min(u16x16(1.0f), u16x16(1.0f)), 1.0f);
    check_near(u16x16::max(u16x16(0.0f), u16x16(5.0f)), 5.0f);
}

FL_TEST_CASE("u16x16 - mod") {
    check_near(u16x16::mod(u16x16(5.5f), u16x16(2.0f)), 1.5f);
    check_near(u16x16::mod(u16x16(3.0f), u16x16(1.5f)), 0.0f);
    check_near(u16x16::mod(u16x16(1.0f), u16x16(3.0f)), 1.0f);
}

// ---------------------------------------------------------------------------
// Interpolation and clamping
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - lerp, clamp, step, smoothstep") {
    // lerp
    check_near(u16x16::lerp(u16x16(0.0f), u16x16(4.0f), u16x16(0.5f)), 2.0f);
    check_near(u16x16::lerp(u16x16(1.0f), u16x16(3.0f), u16x16(0.0f)), 1.0f);
    check_near(u16x16::lerp(u16x16(1.0f), u16x16(3.0f), u16x16(1.0f)), 3.0f);
    check_near(u16x16::lerp(u16x16(0.0f), u16x16(10.0f), u16x16(0.5f)), 5.0f, tol() * 10.0f);

    // clamp
    check_near(u16x16::clamp(u16x16(5.0f), u16x16(0.0f), u16x16(3.0f)), 3.0f);
    check_near(u16x16::clamp(u16x16(0.5f), u16x16(1.0f), u16x16(3.0f)), 1.0f);
    check_near(u16x16::clamp(u16x16(1.5f), u16x16(0.0f), u16x16(3.0f)), 1.5f);

    // step
    check_near(u16x16::step(u16x16(1.0f), u16x16(0.5f)), 0.0f);
    check_near(u16x16::step(u16x16(1.0f), u16x16(1.5f)), 1.0f);
    check_near(u16x16::step(u16x16(1.0f), u16x16(1.0f)), 1.0f);

    // smoothstep
    check_near(u16x16::smoothstep(u16x16(0.0f), u16x16(1.0f), u16x16(0.0f)), 0.0f, smooth_tol());
    check_near(u16x16::smoothstep(u16x16(0.0f), u16x16(1.0f), u16x16(1.0f)), 1.0f, smooth_tol());
    check_near(u16x16::smoothstep(u16x16(0.0f), u16x16(1.0f), u16x16(0.5f)), 0.5f, smooth_tol());
}

// ---------------------------------------------------------------------------
// Edge values
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - edge values") {
    // Maximum representable integer part (65535)
    constexpr u32 max_int = (1u << u16x16::INT_BITS) - 1;
    u16x16 big(static_cast<float>(max_int));
    FL_CHECK_EQ(big.to_int(), max_int);

    // Smallest positive fraction
    u16x16 tiny = u16x16::from_raw(u32(1));
    FL_CHECK_EQ(tiny.raw(), u32(1));
    FL_CHECK_EQ(tiny.to_int(), u32(0));
    FL_CHECK(tiny > u16x16());

    // Zero
    u16x16 zero;
    FL_CHECK_EQ(zero.raw(), u32(0));
    FL_CHECK_EQ(zero.to_int(), u32(0));

    // Large value tests
    u16x16 thousand(1000.0f);
    FL_CHECK_EQ(thousand.to_int(), u32(1000));
    check_near(thousand, 1000.0f, tol() * 100.0f);

    u16x16 ten_thousand(10000.0f);
    FL_CHECK_EQ(ten_thousand.to_int(), u32(10000));
}

// ---------------------------------------------------------------------------
// Range tests (unsigned specific)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u16x16 - unsigned range") {
    // Range is [0, 65536) for integer part
    u16x16 zero(0.0f);
    check_near(zero, 0.0f);

    u16x16 small(0.0001f);
    FL_CHECK(small > zero);

    u16x16 large(65535.0f);
    FL_CHECK(large.to_int() == u32(65535));

    // Test full range
    u16x16 mid(32768.0f);
    FL_CHECK_EQ(mid.to_int(), u32(32768));
    check_near(mid, 32768.0f, tol() * 1000.0f);
}

FL_TEST_CASE("u16x16 - fractional precision") {
    // 16 fractional bits means resolution of 1/65536 ≈ 0.0000152587890625
    u16x16 a(1.0f);
    u16x16 b(1.0f + 1.0f/65536.0f);

    // These should be distinguishable
    FL_CHECK(b > a);
    FL_CHECK(b.raw() == a.raw() + 1);

    // Test multiple fractional values
    u16x16 quarter(0.25f);
    u16x16 eighth(0.125f);
    u16x16 sixteenth(0.0625f);

    check_near(quarter, 0.25f);
    check_near(eighth, 0.125f);
    check_near(sixteenth, 0.0625f);
}




} // anonymous namespace

} // FL_TEST_FILE
