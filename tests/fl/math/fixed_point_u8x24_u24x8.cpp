// u8x24 and u24x8 unsigned fixed-point type tests
// Extracted from fixed_point.cpp (sub-issue of #3127).

#include "test.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point.h"
#include "fl/math/math.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace { // Anonymous namespace for u8x24+u24x8 tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// u8x24 tests (from u8x24.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u8x24 construction") {
    FL_SUBCASE("default constructor") {
        u8x24 x;
        FL_CHECK_EQ(x.raw(), 0u);
        FL_CHECK_EQ(x.to_int(), 0u);
        FL_CHECK_EQ(x.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("from float") {
        u8x24 x(1.5f);
        FL_CHECK_EQ(x.to_float(), doctest::Approx(1.5f));

        u8x24 y(0.25f);
        FL_CHECK_EQ(y.to_float(), doctest::Approx(0.25f));

        u8x24 z(255.75f);
        FL_CHECK_EQ(z.to_float(), doctest::Approx(255.75f));
    }

    FL_SUBCASE("from_raw") {
        u8x24 x = u8x24::from_raw(1u << 24);  // 1.0
        FL_CHECK_EQ(x.to_float(), doctest::Approx(1.0f));

        u8x24 y = u8x24::from_raw(1u << 23);  // 0.5
        FL_CHECK_EQ(y.to_float(), doctest::Approx(0.5f));

        u8x24 z = u8x24::from_raw(0u);  // 0.0
        FL_CHECK_EQ(z.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("to_int") {
        u8x24 x(3.7f);
        FL_CHECK_EQ(x.to_int(), 3u);

        u8x24 y(0.9f);
        FL_CHECK_EQ(y.to_int(), 0u);

        u8x24 z(100.1f);
        FL_CHECK_EQ(z.to_int(), 100u);
    }
}

FL_TEST_CASE("u8x24 addition") {
    FL_SUBCASE("basic addition") {
        u8x24 a(1.5f);
        u8x24 b(2.5f);
        u8x24 c = a + b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(4.0f));
    }

    FL_SUBCASE("fractional addition") {
        u8x24 a(0.25f);
        u8x24 b(0.75f);
        u8x24 c = a + b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(1.0f));
    }

    FL_SUBCASE("zero addition") {
        u8x24 a(5.0f);
        u8x24 b(0.0f);
        u8x24 c = a + b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(5.0f));
    }
}

FL_TEST_CASE("u8x24 subtraction") {
    FL_SUBCASE("basic subtraction") {
        u8x24 a(5.5f);
        u8x24 b(2.5f);
        u8x24 c = a - b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("fractional subtraction") {
        u8x24 a(1.0f);
        u8x24 b(0.25f);
        u8x24 c = a - b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.75f));
    }

    FL_SUBCASE("zero result") {
        u8x24 a(3.0f);
        u8x24 b(3.0f);
        u8x24 c = a - b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("underflow wraps (unsigned behavior)") {
        u8x24 a(1.0f);
        u8x24 b(2.0f);
        u8x24 c = a - b;
        // Unsigned underflow wraps around
        FL_CHECK_GT(c.raw(), (1u << 30));  // Very large value
    }
}

FL_TEST_CASE("u8x24 multiplication") {
    FL_SUBCASE("basic multiplication") {
        u8x24 a(2.0f);
        u8x24 b(3.0f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(6.0f));
    }

    FL_SUBCASE("fractional multiplication") {
        u8x24 a(1.5f);
        u8x24 b(2.5f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.75f));
    }

    FL_SUBCASE("zero multiplication") {
        u8x24 a(5.0f);
        u8x24 b(0.0f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("multiplication by one") {
        u8x24 a(7.5f);
        u8x24 b(1.0f);
        u8x24 c = a * b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(7.5f));
    }
}

FL_TEST_CASE("u8x24 division") {
    FL_SUBCASE("basic division") {
        u8x24 a(6.0f);
        u8x24 b(2.0f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("fractional division") {
        u8x24 a(7.5f);
        u8x24 b(2.5f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("division by one") {
        u8x24 a(9.0f);
        u8x24 b(1.0f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(9.0f));
    }

    FL_SUBCASE("division result less than one") {
        u8x24 a(1.0f);
        u8x24 b(4.0f);
        u8x24 c = a / b;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(0.25f));
    }
}

FL_TEST_CASE("u8x24 scalar multiplication") {
    FL_SUBCASE("multiply by scalar") {
        u8x24 a(2.5f);
        u8x24 b = a * 3u;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(7.5f));
    }

    FL_SUBCASE("scalar multiply commutative") {
        u8x24 a(1.5f);
        u8x24 b = 4u * a;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(6.0f));
    }

    FL_SUBCASE("scalar multiply by zero") {
        u8x24 a(5.0f);
        u8x24 b = a * 0u;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("scalar multiply by one") {
        u8x24 a(3.7f);
        u8x24 b = a * 1u;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(3.7f));
    }
}

FL_TEST_CASE("u8x24 shift operations") {
    FL_SUBCASE("right shift") {
        u8x24 a(8.0f);
        u8x24 b = a >> 1;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(4.0f));

        u8x24 c = a >> 2;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(2.0f));
    }

    FL_SUBCASE("left shift") {
        u8x24 a(2.0f);
        u8x24 b = a << 1;
        FL_CHECK_EQ(b.to_float(), doctest::Approx(4.0f));

        u8x24 c = a << 2;
        FL_CHECK_EQ(c.to_float(), doctest::Approx(8.0f));
    }
}

FL_TEST_CASE("u8x24 comparisons") {
    FL_SUBCASE("less than") {
        u8x24 a(1.0f);
        u8x24 b(2.0f);
        FL_CHECK(a < b);
        FL_CHECK_FALSE(b < a);
        FL_CHECK_FALSE(a < a);
    }

    FL_SUBCASE("greater than") {
        u8x24 a(3.0f);
        u8x24 b(1.5f);
        FL_CHECK(a > b);
        FL_CHECK_FALSE(b > a);
        FL_CHECK_FALSE(a > a);
    }

    FL_SUBCASE("less than or equal") {
        u8x24 a(2.0f);
        u8x24 b(3.0f);
        FL_CHECK(a <= b);
        FL_CHECK(a <= a);
        FL_CHECK_FALSE(b <= a);
    }

    FL_SUBCASE("greater than or equal") {
        u8x24 a(5.0f);
        u8x24 b(3.0f);
        FL_CHECK(a >= b);
        FL_CHECK(a >= a);
        FL_CHECK_FALSE(b >= a);
    }

    FL_SUBCASE("equality") {
        u8x24 a(4.5f);
        u8x24 b(4.5f);
        u8x24 c(4.6f);
        FL_CHECK(a == b);
        FL_CHECK_FALSE(a == c);
    }

    FL_SUBCASE("inequality") {
        u8x24 a(1.0f);
        u8x24 b(2.0f);
        FL_CHECK(a != b);
        FL_CHECK_FALSE(a != a);
    }
}

FL_TEST_CASE("u8x24 math functions") {
    FL_SUBCASE("mod") {
        u8x24 a(7.5f);
        u8x24 b(3.0f);
        u8x24 c = u8x24::mod(a, b);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(1.5f));
    }

    FL_SUBCASE("floor") {
        u8x24 a(3.7f);
        u8x24 b = u8x24::floor(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(3.0f));

        u8x24 c(5.0f);
        u8x24 d = u8x24::floor(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(5.0f));
    }

    FL_SUBCASE("ceil") {
        u8x24 a(3.2f);
        u8x24 b = u8x24::ceil(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(4.0f));

        u8x24 c(5.0f);
        u8x24 d = u8x24::ceil(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(5.0f));
    }

    FL_SUBCASE("fract") {
        u8x24 a(3.75f);
        u8x24 b = u8x24::fract(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.75f));

        u8x24 c(5.0f);
        u8x24 d = u8x24::fract(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("min") {
        u8x24 a(3.0f);
        u8x24 b(5.0f);
        u8x24 c = u8x24::min(a, b);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(3.0f));

        u8x24 d = u8x24::min(b, a);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(3.0f));
    }

    FL_SUBCASE("max") {
        u8x24 a(3.0f);
        u8x24 b(5.0f);
        u8x24 c = u8x24::max(a, b);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(5.0f));

        u8x24 d = u8x24::max(b, a);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(5.0f));
    }

    FL_SUBCASE("clamp") {
        u8x24 lo(1.0f);
        u8x24 hi(10.0f);

        u8x24 a(5.0f);
        FL_CHECK_EQ(u8x24::clamp(a, lo, hi).to_float(), doctest::Approx(5.0f));

        u8x24 b(0.5f);
        FL_CHECK_EQ(u8x24::clamp(b, lo, hi).to_float(), doctest::Approx(1.0f));

        u8x24 c(15.0f);
        FL_CHECK_EQ(u8x24::clamp(c, lo, hi).to_float(), doctest::Approx(10.0f));
    }

    FL_SUBCASE("lerp") {
        u8x24 a(0.0f);
        u8x24 b(10.0f);
        u8x24 t(0.5f);
        u8x24 c = u8x24::lerp(a, b, t);
        FL_CHECK_EQ(c.to_float(), doctest::Approx(5.0f));

        u8x24 t0(0.0f);
        u8x24 d = u8x24::lerp(a, b, t0);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(0.0f));

        u8x24 t1(1.0f);
        u8x24 e = u8x24::lerp(a, b, t1);
        FL_CHECK_EQ(e.to_float(), doctest::Approx(10.0f));
    }

    FL_SUBCASE("step") {
        u8x24 edge(5.0f);
        u8x24 a(3.0f);
        u8x24 b(7.0f);

        FL_CHECK_EQ(u8x24::step(edge, a).to_float(), doctest::Approx(0.0f));
        FL_CHECK_EQ(u8x24::step(edge, b).to_float(), doctest::Approx(1.0f));
    }

    FL_SUBCASE("smoothstep") {
        u8x24 edge0(0.0f);
        u8x24 edge1(1.0f);
        u8x24 x(0.5f);
        u8x24 result = u8x24::smoothstep(edge0, edge1, x);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(0.5f).epsilon(0.01f));
    }
}

FL_TEST_CASE("u8x24 sqrt") {
    FL_SUBCASE("sqrt of perfect squares") {
        u8x24 a(4.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(2.0f).epsilon(0.001f));

        u8x24 c(9.0f);
        u8x24 d = u8x24::sqrt(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(3.0f).epsilon(0.001f));
    }

    FL_SUBCASE("sqrt of non-perfect squares") {
        u8x24 a(2.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(1.414f).epsilon(0.01f));

        u8x24 c(10.0f);
        u8x24 d = u8x24::sqrt(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(3.162f).epsilon(0.01f));
    }

    FL_SUBCASE("sqrt of zero") {
        u8x24 a(0.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("sqrt of one") {
        u8x24 a(1.0f);
        u8x24 b = u8x24::sqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(1.0f).epsilon(0.001f));
    }
}

FL_TEST_CASE("u8x24 rsqrt") {
    FL_SUBCASE("rsqrt of perfect squares") {
        u8x24 a(4.0f);
        u8x24 b = u8x24::rsqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.5f).epsilon(0.01f));

        u8x24 c(9.0f);
        u8x24 d = u8x24::rsqrt(c);
        FL_CHECK_EQ(d.to_float(), doctest::Approx(0.333f).epsilon(0.01f));
    }

    FL_SUBCASE("rsqrt of one") {
        u8x24 a(1.0f);
        u8x24 b = u8x24::rsqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(1.0f).epsilon(0.01f));
    }

    FL_SUBCASE("rsqrt of zero returns zero") {
        u8x24 a(0.0f);
        u8x24 b = u8x24::rsqrt(a);
        FL_CHECK_EQ(b.to_float(), doctest::Approx(0.0f));
    }
}

FL_TEST_CASE("u8x24 pow") {
    FL_SUBCASE("integer exponents") {
        u8x24 base(2.0f);
        u8x24 exp(3.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(8.0f).epsilon(0.1f));
    }

    FL_SUBCASE("fractional exponents") {
        u8x24 base(4.0f);
        u8x24 exp(0.5f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(2.0f).epsilon(0.1f));
    }

    FL_SUBCASE("exponent of zero") {
        u8x24 base(5.0f);
        u8x24 exp(0.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(1.0f).epsilon(0.01f));
    }

    FL_SUBCASE("base of one") {
        u8x24 base(1.0f);
        u8x24 exp(100.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(1.0f).epsilon(0.01f));
    }

    FL_SUBCASE("base of zero") {
        u8x24 base(0.0f);
        u8x24 exp(5.0f);
        u8x24 result = u8x24::pow(base, exp);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(0.0f));
    }
}

FL_TEST_CASE("u8x24 edge cases") {
    FL_SUBCASE("zero value") {
        u8x24 zero;
        FL_CHECK_EQ(zero.raw(), 0u);
        FL_CHECK_EQ(zero.to_int(), 0u);
        FL_CHECK_EQ(zero.to_float(), doctest::Approx(0.0f));
    }

    FL_SUBCASE("maximum integer value (255)") {
        u8x24 max(255.0f);
        FL_CHECK_EQ(max.to_int(), 255u);
        FL_CHECK_EQ(max.to_float(), doctest::Approx(255.0f));
    }

    FL_SUBCASE("maximum fractional value (almost 256)") {
        // 8 integer bits allows values from 0 to just under 256
        // Maximum raw value: 0xFFFFFFFF represents 256.0 - 2^-24
        u32 max_raw = 0xFFFFFFFFu;
        u8x24 max = u8x24::from_raw(max_raw);
        FL_CHECK_GT(max.to_float(), 255.9f);
        // Due to float precision, this may round to exactly 256.0f
        FL_CHECK_LE(max.to_float(), 256.0f);
    }

    FL_SUBCASE("small fractional values") {
        u8x24 tiny(0.0001f);
        FL_CHECK_GT(tiny.to_float(), 0.0f);
        FL_CHECK_LT(tiny.to_float(), 0.001f);
    }
}

FL_TEST_CASE("u8x24 complex expressions") {
    FL_SUBCASE("combined operations") {
        u8x24 a(2.0f);
        u8x24 b(3.0f);
        u8x24 c(4.0f);
        u8x24 result = (a + b) * c;
        FL_CHECK_EQ(result.to_float(), doctest::Approx(20.0f));
    }

    FL_SUBCASE("nested operations") {
        u8x24 a(10.0f);
        u8x24 b(2.0f);
        u8x24 c(3.0f);
        u8x24 result = a / (b + c);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(2.0f));
    }

    FL_SUBCASE("mixed operations") {
        u8x24 a(5.0f);
        u8x24 b(2.0f);
        u8x24 result = (a * b) - (a / b);
        FL_CHECK_EQ(result.to_float(), doctest::Approx(7.5f));
    }
}


// ---------------------------------------------------------------------------
// u24x8 – unsigned 24.8 fixed-point
// ---------------------------------------------------------------------------

FL_TEST_CASE("u24x8 - default construction") {
    u24x8 a;
    FL_CHECK_EQ(a.raw(), u32(0));
    FL_CHECK_EQ(a.to_int(), u32(0));
}

FL_TEST_CASE("u24x8 - float construction") {
    u24x8 one(1.0f);
    FL_CHECK_EQ(one.raw(), u32(1) << 8);
    FL_CHECK_EQ(one.to_int(), u32(1));

    u24x8 half(0.5f);
    FL_CHECK_EQ(half.raw(), u32(1) << 7);
    FL_CHECK_EQ(half.to_int(), u32(0));

    u24x8 val(3.0f);
    FL_CHECK_EQ(val.to_int(), u32(3));

    u24x8 large(100.0f);
    FL_CHECK_EQ(large.to_int(), u32(100));

    u24x8 max_val(16777215.0f);  // ~2^24 - 1
    FL_CHECK_GT(max_val.raw(), u32(0));
}

FL_TEST_CASE("u24x8 - from_raw") {
    // 1.0
    u24x8 a = u24x8::from_raw(u32(1) << 8);
    FL_CHECK_EQ(a.to_int(), u32(1));

    // 2.5
    u32 raw_2_5 = (u32(2) << 8) + (u32(1) << 7);
    u24x8 b = u24x8::from_raw(raw_2_5);
    FL_CHECK_EQ(b.to_int(), u32(2));
    FL_CHECK_CLOSE(b.to_float(), 2.5f, 0.01f);

    // smallest positive fraction
    u24x8 c = u24x8::from_raw(u32(1));
    FL_CHECK_EQ(c.raw(), u32(1));
    FL_CHECK_EQ(c.to_int(), u32(0));
    FL_CHECK(c > u24x8());
}

FL_TEST_CASE("u24x8 - addition") {
    u24x8 a(1.0f), b(2.0f);
    u24x8 c = a + b;
    FL_CHECK_EQ(c.to_int(), u32(3));
    FL_CHECK_CLOSE(c.to_float(), 3.0f, 0.01f);

    // Fractional
    u24x8 d(0.25f), e(0.75f);
    FL_CHECK_CLOSE((d + e).to_float(), 1.0f, 0.01f);

    // Zero identity
    u24x8 zero;
    FL_CHECK_EQ((a + zero).raw(), a.raw());
}

FL_TEST_CASE("u24x8 - subtraction") {
    u24x8 a(3.0f), b(1.0f);
    FL_CHECK_CLOSE((a - b).to_float(), 2.0f, 0.01f);

    // Self subtraction
    u24x8 zero;
    FL_CHECK_EQ((a - a).raw(), zero.raw());

    // Fractional
    u24x8 c(1.75f), d(0.25f);
    FL_CHECK_CLOSE((c - d).to_float(), 1.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - fixed-point multiply") {
    u24x8 a(2.0f), b(3.0f);
    FL_CHECK_CLOSE((a * b).to_float(), 6.0f, 0.01f);

    // 0.5 * 0.5 = 0.25
    u24x8 half(0.5f);
    FL_CHECK_CLOSE((half * half).to_float(), 0.25f, 0.01f);

    // Multiply by 1 = identity
    u24x8 one(1.0f);
    FL_CHECK_EQ((a * one).raw(), a.raw());

    // Multiply by 0 = zero
    u24x8 zero;
    FL_CHECK_EQ((a * zero).raw(), u32(0));

    // Fractional precision
    u24x8 c(1.5f), d(2.0f);
    FL_CHECK_CLOSE((c * d).to_float(), 3.0f, 0.01f);

    // Higher fractional precision
    u24x8 e(1.5f), f(2.5f);
    FL_CHECK_CLOSE((e * f).to_float(), 3.75f, 0.01f);
}

FL_TEST_CASE("u24x8 - fixed-point divide") {
    u24x8 a(6.0f), b(3.0f);
    FL_CHECK_CLOSE((a / b).to_float(), 2.0f, 0.01f);

    u24x8 one(1.0f), two(2.0f);
    FL_CHECK_CLOSE((one / two).to_float(), 0.5f, 0.01f);

    u24x8 four(4.0f);
    FL_CHECK_CLOSE((one / four).to_float(), 0.25f, 0.01f);

    // Divide by 1 = identity
    FL_CHECK_EQ((a / one).raw(), a.raw());

    // Fractional result: 1/3
    u24x8 three(3.0f);
    FL_CHECK_CLOSE((one / three).to_float(), 0.3333f, 0.1f);
}

FL_TEST_CASE("u24x8 - scalar multiply") {
    u24x8 a(1.5f);

    // fp * scalar
    FL_CHECK_CLOSE((a * u32(2)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE((a * u32(0)).to_float(), 0.0f, 0.01f);

    // scalar * fp (commutative friend)
    FL_CHECK_CLOSE((u32(2) * a).to_float(), 3.0f, 0.01f);

    // Large scalar multiply
    FL_CHECK_CLOSE((a * u32(100)).to_float(), 150.0f, 0.1f);
}

FL_TEST_CASE("u24x8 - right shift") {
    u24x8 a(4.0f);
    FL_CHECK_CLOSE((a >> 1).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE((a >> 2).to_float(), 1.0f, 0.01f);

    // Fractional shift
    u24x8 b(1.0f);
    FL_CHECK_CLOSE((b >> 1).to_float(), 0.5f, 0.01f);
    FL_CHECK_CLOSE((b >> 2).to_float(), 0.25f, 0.01f);
}

FL_TEST_CASE("u24x8 - left shift") {
    u24x8 a(1.0f);
    FL_CHECK_CLOSE((a << 1).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE((a << 2).to_float(), 4.0f, 0.01f);

    // Fractional shift
    u24x8 b(0.25f);
    FL_CHECK_CLOSE((b << 1).to_float(), 0.5f, 0.01f);
    FL_CHECK_CLOSE((b << 2).to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - comparisons") {
    u24x8 a(1.0f), b(2.0f), c(1.0f);
    u24x8 zero;

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

FL_TEST_CASE("u24x8 - mixed arithmetic expressions") {
    u24x8 a(2.0f), b(3.0f), c(0.5f);
    FL_CHECK_CLOSE(((a + b) * c).to_float(), 2.5f, 0.01f);

    u24x8 d(4.0f);
    FL_CHECK_CLOSE((a * b - c * d).to_float(), 4.0f, 0.01f); // 6 - 2 = 4

    // Round-trip: (a / b) * b ~ a
    FL_CHECK_CLOSE(((a / b) * b).to_float(), 2.0f, 0.02f);

    // Chained
    u24x8 one(1.0f);
    u24x8 result = (a + b) * c / one;
    FL_CHECK_CLOSE(result.to_float(), 2.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - to_float") {
    u24x8 one(1.0f);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, 0.01f);

    u24x8 half(0.5f);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, 0.01f);

    u24x8 large(100.5f);
    FL_CHECK_CLOSE(large.to_float(), 100.5f, 0.01f);

    u24x8 zero;
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - sqrt") {
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(4.0f)).to_float(), 2.0f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(1.0f)).to_float(), 1.0f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(2.0f)).to_float(), 1.4142f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(9.0f)).to_float(), 3.0f, 0.03f);
    FL_CHECK_CLOSE(u24x8::sqrt(u24x8(16.0f)).to_float(), 4.0f, 0.03f);

    FL_CHECK_EQ(u24x8::sqrt(u24x8()).raw(), u32(0)); // sqrt(0) = 0
}

FL_TEST_CASE("u24x8 - rsqrt") {
    FL_CHECK_CLOSE(u24x8::rsqrt(u24x8(4.0f)).to_float(), 0.5f, 0.03f);
    FL_CHECK_CLOSE(u24x8::rsqrt(u24x8(1.0f)).to_float(), 1.0f, 0.03f);

    FL_CHECK_EQ(u24x8::rsqrt(u24x8()).raw(), u32(0)); // rsqrt(0) = 0
}

FL_TEST_CASE("u24x8 - pow") {
    // 2^2 = 4
    FL_CHECK_CLOSE(u24x8::pow(u24x8(2.0f), u24x8(2.0f)).to_float(), 4.0f, 0.1f);

    // 4^0.5 = 2 (sqrt via pow)
    FL_CHECK_CLOSE(u24x8::pow(u24x8(4.0f), u24x8(0.5f)).to_float(), 2.0f, 0.1f);

    // x^0 = 1
    FL_CHECK_CLOSE(u24x8::pow(u24x8(5.0f), u24x8(0.0f)).to_float(), 1.0f, 0.1f);

    // 0^x = 0
    FL_CHECK_EQ(u24x8::pow(u24x8(), u24x8(2.0f)).raw(), u32(0));

    // 2^3 = 8
    FL_CHECK_CLOSE(u24x8::pow(u24x8(2.0f), u24x8(3.0f)).to_float(), 8.0f, 0.1f);
}

FL_TEST_CASE("u24x8 - sqrt identity") {
    float values[] = {1.0f, 2.0f, 4.0f, 9.0f, 16.0f, 25.0f};
    for (float v : values) {
        u24x8 x(v);
        u24x8 s = u24x8::sqrt(x);
        u24x8 sq = s * s;
        FL_CHECK_CLOSE(sq.to_float(), v, v * 0.05f + 0.1f);
    }
}

FL_TEST_CASE("u24x8 - floor and ceil") {
    FL_CHECK_CLOSE(u24x8::floor(u24x8(2.75f)).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::ceil(u24x8(2.75f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::floor(u24x8(3.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::ceil(u24x8(3.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::floor(u24x8(0.25f)).to_float(), 0.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::ceil(u24x8(0.25f)).to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - fract") {
    FL_CHECK_CLOSE(u24x8::fract(u24x8(2.75f)).to_float(), 0.75f, 0.01f);
    FL_CHECK_EQ(u24x8::fract(u24x8(1.0f)).raw(), u32(0));
    FL_CHECK_CLOSE(u24x8::fract(u24x8(0.5f)).to_float(), 0.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - abs") {
    // For unsigned, abs is identity
    FL_CHECK_CLOSE(u24x8::abs(u24x8(3.5f)).to_float(), 3.5f, 0.01f);
    FL_CHECK_EQ(u24x8::abs(u24x8()).raw(), u32(0));
}

FL_TEST_CASE("u24x8 - min and max") {
    u24x8 a(5.0f), b(10.0f);
    FL_CHECK_CLOSE(u24x8::min(a, b).to_float(), 5.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::max(a, b).to_float(), 10.0f, 0.01f);

    u24x8 c(7.5f);
    FL_CHECK_CLOSE(u24x8::min(a, c).to_float(), 5.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::max(a, c).to_float(), 7.5f, 0.01f);
}

FL_TEST_CASE("u24x8 - mod") {
    FL_CHECK_CLOSE(u24x8::mod(u24x8(5.5f), u24x8(2.0f)).to_float(), 1.5f, 0.01f);
    FL_CHECK_CLOSE(u24x8::mod(u24x8(3.0f), u24x8(1.5f)).to_float(), 0.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::mod(u24x8(1.0f), u24x8(3.0f)).to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u24x8 - lerp, clamp, step, smoothstep") {
    // lerp (only works correctly when b >= a for unsigned types)
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(0.0f), u24x8(4.0f), u24x8(0.5f)).to_float(), 2.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(1.0f), u24x8(3.0f), u24x8(0.0f)).to_float(), 1.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(1.0f), u24x8(3.0f), u24x8(1.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::lerp(u24x8(0.0f), u24x8(10.0f), u24x8(0.5f)).to_float(), 5.0f, 0.1f);

    // clamp
    FL_CHECK_CLOSE(u24x8::clamp(u24x8(5.0f), u24x8(0.0f), u24x8(3.0f)).to_float(), 3.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::clamp(u24x8(0.0f), u24x8(1.0f), u24x8(3.0f)).to_float(), 1.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::clamp(u24x8(1.5f), u24x8(0.0f), u24x8(3.0f)).to_float(), 1.5f, 0.01f);

    // step
    FL_CHECK_CLOSE(u24x8::step(u24x8(1.0f), u24x8(0.5f)).to_float(), 0.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::step(u24x8(1.0f), u24x8(1.5f)).to_float(), 1.0f, 0.01f);
    FL_CHECK_CLOSE(u24x8::step(u24x8(1.0f), u24x8(1.0f)).to_float(), 1.0f, 0.01f);

    // smoothstep
    FL_CHECK_CLOSE(u24x8::smoothstep(u24x8(0.0f), u24x8(1.0f), u24x8(0.0f)).to_float(), 0.0f, 0.04f);
    FL_CHECK_CLOSE(u24x8::smoothstep(u24x8(0.0f), u24x8(1.0f), u24x8(1.0f)).to_float(), 1.0f, 0.04f);
    FL_CHECK_CLOSE(u24x8::smoothstep(u24x8(0.0f), u24x8(1.0f), u24x8(0.5f)).to_float(), 0.5f, 0.04f);
}

FL_TEST_CASE("u24x8 - edge values") {
    constexpr u32 max_int = (1u << 24) - 1;

    u24x8 big(static_cast<float>(max_int));
    FL_CHECK_GT(big.to_int(), u32(0));

    // Smallest positive fraction
    u24x8 tiny = u24x8::from_raw(u32(1));
    FL_CHECK_EQ(tiny.raw(), u32(1));
    FL_CHECK_EQ(tiny.to_int(), u32(0));
    FL_CHECK(tiny > u24x8());

    // Zero
    u24x8 zero;
    FL_CHECK_EQ(zero.raw(), u32(0));
    FL_CHECK_EQ(zero.to_int(), u32(0));
}

FL_TEST_CASE("u24x8 - large value arithmetic") {
    u24x8 a(1000.0f), b(500.0f);
    FL_CHECK_CLOSE((a + b).to_float(), 1500.0f, 1.0f);
    FL_CHECK_CLOSE((a - b).to_float(), 500.0f, 1.0f);
    FL_CHECK_CLOSE((a / b).to_float(), 2.0f, 0.01f);
}

} // anonymous namespace

} // FL_TEST_FILE
