#include "fl/fixed_point/s8x24.h"
#include "doctest.h"

using fl::s8x24;

// Helper: check that a s8x24 value is close to a float within tolerance.
static void check_near(s8x24 val, float expected, float tol = 0.0001f) {
    // Convert raw back to float for comparison.
    float actual = static_cast<float>(val.raw()) / (1 << s8x24::FRAC_BITS);
    float diff = actual - expected;
    if (diff < 0) diff = -diff;
    CHECK_MESSAGE(diff <= tol,
                  "expected ~", expected, " got ", actual, " (diff=", diff, ")");
}

TEST_CASE("s8x24 - default construction") {
    s8x24 a;
    CHECK(a.raw() == 0);
    CHECK(a.to_int() == 0);
}

TEST_CASE("s8x24 - float construction") {
    s8x24 one(1.0f);
    CHECK(one.raw() == (1 << 24));
    CHECK(one.to_int() == 1);

    s8x24 half(0.5f);
    CHECK(half.raw() == (1 << 23));
    CHECK(half.to_int() == 0);

    s8x24 neg(-1.0f);
    CHECK(neg.raw() == -(1 << 24));
    CHECK(neg.to_int() == -1);

    s8x24 neg_half(-0.5f);
    CHECK(neg_half.to_int() == -1); // floor(-0.5) = -1 via arithmetic shift

    s8x24 small_val(3.0f);
    CHECK(small_val.to_int() == 3);

    s8x24 neg_small(-3.0f);
    CHECK(neg_small.to_int() == -3);
}

TEST_CASE("s8x24 - from_raw") {
    s8x24 a = s8x24::from_raw(0x01000000); // 1.0
    CHECK(a.to_int() == 1);

    s8x24 b = s8x24::from_raw(0x02800000); // 2.5
    CHECK(b.to_int() == 2);
    check_near(b, 2.5f);

    s8x24 c = s8x24::from_raw(-1); // smallest negative fraction
    CHECK(c.raw() == -1);
    CHECK(c.to_int() == -1); // arithmetic shift
}

TEST_CASE("s8x24 - addition") {
    s8x24 a(1.0f);
    s8x24 b(2.0f);
    s8x24 c = a + b;
    CHECK(c.to_int() == 3);
    check_near(c, 3.0f);

    // Fractional addition
    s8x24 d(0.25f);
    s8x24 e(0.75f);
    check_near(d + e, 1.0f);

    // Negative addition
    s8x24 f(-3.0f);
    check_near(a + f, -2.0f);

    // Zero identity
    s8x24 zero;
    CHECK((a + zero).raw() == a.raw());
}

TEST_CASE("s8x24 - subtraction") {
    s8x24 a(5.0f);
    s8x24 b(3.0f);
    check_near(a - b, 2.0f);

    // Result crosses zero
    check_near(b - a, -2.0f);

    // Self subtraction
    s8x24 zero;
    CHECK((a - a).raw() == zero.raw());

    // Fractional
    s8x24 c(1.75f);
    s8x24 d(0.25f);
    check_near(c - d, 1.5f);
}

TEST_CASE("s8x24 - unary negation") {
    s8x24 a(3.5f);
    s8x24 neg_a = -a;
    check_near(neg_a, -3.5f);

    // Double negation
    CHECK((-neg_a).raw() == a.raw());

    // Negate zero
    s8x24 zero;
    CHECK((-zero).raw() == 0);
}

TEST_CASE("s8x24 - fixed-point multiply") {
    // 2 * 3 = 6
    s8x24 a(2.0f);
    s8x24 b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    s8x24 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 is identity
    s8x24 one(1.0f);
    CHECK((a * one).raw() == a.raw());

    // Multiply by 0 is zero
    s8x24 zero;
    CHECK((a * zero).raw() == 0);

    // Negative * positive
    s8x24 neg(-2.0f);
    check_near(neg * b, -6.0f);

    // Negative * negative
    check_near(neg * s8x24(-3.0f), 6.0f);

    // Fractional precision
    s8x24 c(1.5f);
    s8x24 d(2.5f);
    check_near(c * d, 3.75f);
}

TEST_CASE("s8x24 - fixed-point divide") {
    // 6 / 3 = 2
    s8x24 a(6.0f);
    s8x24 b(3.0f);
    check_near(a / b, 2.0f);

    // 1 / 2 = 0.5
    s8x24 one(1.0f);
    s8x24 two(2.0f);
    check_near(one / two, 0.5f);

    // 1 / 4 = 0.25
    s8x24 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 is identity
    CHECK((a / one).raw() == a.raw());

    // Negative dividend
    s8x24 neg(-6.0f);
    check_near(neg / b, -2.0f);

    // Negative divisor
    check_near(a / s8x24(-3.0f), -2.0f);

    // Both negative
    check_near(neg / s8x24(-3.0f), 2.0f);

    // Fractional result
    s8x24 three(3.0f);
    check_near(one / three, 0.3333f, 0.001f);
}

TEST_CASE("s8x24 - scalar multiply") {
    s8x24 a(1.5f);

    // fp * scalar (keep values in [-128, 127] range)
    check_near(a * 2, 3.0f);
    check_near(a * 0, 0.0f);
    check_near(a * -1, -1.5f);

    // scalar * fp (commutative friend)
    check_near(2 * a, 3.0f);
    check_near(-3 * a, -4.5f);
}

TEST_CASE("s8x24 - right shift") {
    s8x24 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    s8x24 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);

    // Negative shift preserves sign (arithmetic shift)
    s8x24 neg(-4.0f);
    check_near(neg >> 1, -2.0f);
    check_near(neg >> 2, -1.0f);
}

TEST_CASE("s8x24 - comparisons") {
    s8x24 a(1.0f);
    s8x24 b(2.0f);
    s8x24 c(1.0f);
    s8x24 neg(-1.0f);
    s8x24 zero;

    // operator==, operator!=
    CHECK(a == c);
    CHECK(a != b);
    CHECK(!(a == b));
    CHECK(!(a != c));

    // operator<, operator>
    CHECK(a < b);
    CHECK(b > a);
    CHECK(!(b < a));
    CHECK(!(a > b));

    // operator<=, operator>=
    CHECK(a <= b);
    CHECK(a <= c);
    CHECK(b >= a);
    CHECK(a >= c);

    // Negative comparisons
    CHECK(neg < zero);
    CHECK(neg < a);
    CHECK(zero > neg);
    CHECK(a > neg);
}

TEST_CASE("s8x24 - sin") {
    // sin(0) = 0
    s8x24 zero;
    check_near(s8x24::sin(zero), 0.0f, 0.01f);

    // sin(pi/2) ~ 1
    s8x24 half_pi(1.5707963f);
    check_near(s8x24::sin(half_pi), 1.0f, 0.01f);

    // sin(pi) ~ 0
    s8x24 pi(3.1415926f);
    check_near(s8x24::sin(pi), 0.0f, 0.02f);

    // sin(-pi/2) ~ -1
    s8x24 neg_half_pi(-1.5707963f);
    check_near(s8x24::sin(neg_half_pi), -1.0f, 0.01f);
}

TEST_CASE("s8x24 - cos") {
    // cos(0) = 1
    s8x24 zero;
    check_near(s8x24::cos(zero), 1.0f, 0.01f);

    // cos(pi/2) ~ 0
    s8x24 half_pi(1.5707963f);
    check_near(s8x24::cos(half_pi), 0.0f, 0.02f);

    // cos(pi) ~ -1
    s8x24 pi(3.1415926f);
    check_near(s8x24::cos(pi), -1.0f, 0.01f);
}

TEST_CASE("s8x24 - sincos") {
    s8x24 angle(0.7854f); // ~pi/4
    s8x24 s, c;
    s8x24::sincos(angle, s, c);

    // sin(pi/4) ~ cos(pi/4) ~ 0.7071
    check_near(s, 0.7071f, 0.02f);
    check_near(c, 0.7071f, 0.02f);

    // sincos should match individual sin/cos
    CHECK(s.raw() == s8x24::sin(angle).raw());
    CHECK(c.raw() == s8x24::cos(angle).raw());
}

TEST_CASE("s8x24 - sin^2 + cos^2 = 1") {
    // Pythagorean identity at several angles
    float angles[] = {0.0f, 0.5f, 1.0f, 1.5707963f, 2.0f, 3.1415926f, -1.0f};
    for (float ang : angles) {
        s8x24 a(ang);
        s8x24 s = s8x24::sin(a);
        s8x24 c = s8x24::cos(a);
        s8x24 sum = s * s + c * c;
        check_near(sum, 1.0f, 0.03f);
    }
}

TEST_CASE("s8x24 - mixed arithmetic expressions") {
    // (a + b) * c
    s8x24 a(2.0f);
    s8x24 b(3.0f);
    s8x24 c(0.5f);
    check_near((a + b) * c, 2.5f);

    // a * b - c * d
    s8x24 d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // (a / b) * b ~ a  (round-trip)
    check_near((a / b) * b, 2.0f, 0.001f);

    // Chained operations
    s8x24 one(1.0f);
    s8x24 result = (a + b) * c / one;
    check_near(result, 2.5f);
}

TEST_CASE("s8x24 - to_float") {
    s8x24 one(1.0f);
    CHECK(one.to_float() == doctest::Approx(1.0f).epsilon(0.001f));

    s8x24 half(0.5f);
    CHECK(half.to_float() == doctest::Approx(0.5f).epsilon(0.001f));

    s8x24 neg(-2.5f);
    CHECK(neg.to_float() == doctest::Approx(-2.5f).epsilon(0.001f));

    s8x24 zero;
    CHECK(zero.to_float() == doctest::Approx(0.0f));
}

TEST_CASE("s8x24 - sqrt") {
    // sqrt(4) = 2
    check_near(s8x24::sqrt(s8x24(4.0f)), 2.0f, 0.001f);

    // sqrt(1) = 1
    check_near(s8x24::sqrt(s8x24(1.0f)), 1.0f, 0.001f);

    // sqrt(2) ~ 1.4142
    check_near(s8x24::sqrt(s8x24(2.0f)), 1.4142f, 0.001f);

    // sqrt(0) = 0
    CHECK(s8x24::sqrt(s8x24()).raw() == 0);

    // sqrt(negative) = 0
    CHECK(s8x24::sqrt(s8x24(-1.0f)).raw() == 0);

    // sqrt(9) = 3
    check_near(s8x24::sqrt(s8x24(9.0f)), 3.0f, 0.001f);
}

TEST_CASE("s8x24 - rsqrt") {
    // rsqrt(4) = 0.5
    check_near(s8x24::rsqrt(s8x24(4.0f)), 0.5f, 0.001f);

    // rsqrt(1) = 1
    check_near(s8x24::rsqrt(s8x24(1.0f)), 1.0f, 0.001f);

    // rsqrt(0) = 0 (sentinel)
    CHECK(s8x24::rsqrt(s8x24()).raw() == 0);

    // rsqrt(negative) = 0
    CHECK(s8x24::rsqrt(s8x24(-1.0f)).raw() == 0);
}

TEST_CASE("s8x24 - pow") {
    // 2^3 = 8
    check_near(s8x24::pow(s8x24(2.0f), s8x24(3.0f)), 8.0f, 0.01f);

    // 4^0.5 = 2 (sqrt via pow)
    check_near(s8x24::pow(s8x24(4.0f), s8x24(0.5f)), 2.0f, 0.01f);

    // x^0 = 1
    check_near(s8x24::pow(s8x24(5.0f), s8x24(0.0f)), 1.0f, 0.01f);

    // 0^x = 0
    CHECK(s8x24::pow(s8x24(), s8x24(2.0f)).raw() == 0);

    // negative base = 0
    CHECK(s8x24::pow(s8x24(-1.0f), s8x24(2.0f)).raw() == 0);
}

TEST_CASE("s8x24 - sqrt identity: sqrt(x)^2 ~ x") {
    float values[] = {1.0f, 2.0f, 4.0f, 9.0f, 0.25f, 100.0f};
    for (float v : values) {
        s8x24 x(v);
        s8x24 s = s8x24::sqrt(x);
        s8x24 sq = s * s;
        check_near(sq, v, v * 0.001f + 0.001f);
    }
}

TEST_CASE("s8x24 - floor and ceil") {
    check_near(s8x24::floor(s8x24(2.75f)), 2.0f);
    check_near(s8x24::ceil(s8x24(2.75f)), 3.0f);
    check_near(s8x24::floor(s8x24(-1.25f)), -2.0f);
    check_near(s8x24::ceil(s8x24(-1.25f)), -1.0f);
    check_near(s8x24::floor(s8x24(3.0f)), 3.0f);
    check_near(s8x24::ceil(s8x24(3.0f)), 3.0f);
}

TEST_CASE("s8x24 - fract") {
    check_near(s8x24::fract(s8x24(2.75f)), 0.75f);
    CHECK(s8x24::fract(s8x24(1.0f)).raw() == 0);
    check_near(s8x24::fract(s8x24(0.5f)), 0.5f);
}

TEST_CASE("s8x24 - abs and sign") {
    check_near(s8x24::abs(s8x24(3.5f)), 3.5f);
    check_near(s8x24::abs(s8x24(-3.5f)), 3.5f);
    CHECK(s8x24::abs(s8x24()).raw() == 0);
    check_near(s8x24::sign(s8x24(5.0f)), 1.0f);
    check_near(s8x24::sign(s8x24(-5.0f)), -1.0f);
    CHECK(s8x24::sign(s8x24()).raw() == 0);
}

TEST_CASE("s8x24 - mod") {
    check_near(s8x24::mod(s8x24(5.5f), s8x24(2.0f)), 1.5f);
    check_near(s8x24::mod(s8x24(3.0f), s8x24(1.5f)), 0.0f);
    check_near(s8x24::mod(s8x24(1.0f), s8x24(3.0f)), 1.0f);
}

TEST_CASE("s8x24 - inverse trig") {
    check_near(s8x24::atan(s8x24(1.0f)), 0.7854f, 0.01f);
    check_near(s8x24::atan(s8x24(0.0f)), 0.0f, 0.001f);
    check_near(s8x24::atan2(s8x24(1.0f), s8x24(1.0f)), 0.7854f, 0.01f);
    check_near(s8x24::asin(s8x24(0.0f)), 0.0f, 0.001f);
    check_near(s8x24::asin(s8x24(1.0f)), 1.5708f, 0.01f);
    check_near(s8x24::acos(s8x24(1.0f)), 0.0f, 0.001f);
    check_near(s8x24::acos(s8x24(0.0f)), 1.5708f, 0.01f);
}

TEST_CASE("s8x24 - lerp, clamp, step, smoothstep") {
    check_near(s8x24::lerp(s8x24(0.0f), s8x24(4.0f), s8x24(0.5f)), 2.0f, 0.001f);
    check_near(s8x24::lerp(s8x24(1.0f), s8x24(3.0f), s8x24(0.0f)), 1.0f);
    check_near(s8x24::lerp(s8x24(1.0f), s8x24(3.0f), s8x24(1.0f)), 3.0f);

    check_near(s8x24::clamp(s8x24(5.0f), s8x24(0.0f), s8x24(3.0f)), 3.0f);
    check_near(s8x24::clamp(s8x24(-1.0f), s8x24(0.0f), s8x24(3.0f)), 0.0f);
    check_near(s8x24::clamp(s8x24(1.5f), s8x24(0.0f), s8x24(3.0f)), 1.5f);

    check_near(s8x24::step(s8x24(1.0f), s8x24(0.5f)), 0.0f);
    check_near(s8x24::step(s8x24(1.0f), s8x24(1.5f)), 1.0f);
    check_near(s8x24::step(s8x24(1.0f), s8x24(1.0f)), 1.0f);

    check_near(s8x24::smoothstep(s8x24(0.0f), s8x24(1.0f), s8x24(-0.5f)), 0.0f, 0.001f);
    check_near(s8x24::smoothstep(s8x24(0.0f), s8x24(1.0f), s8x24(1.5f)), 1.0f, 0.001f);
    check_near(s8x24::smoothstep(s8x24(0.0f), s8x24(1.0f), s8x24(0.5f)), 0.5f, 0.01f);
}

TEST_CASE("s8x24 - edge values") {
    // Largest positive integer representable: 127.x
    s8x24 big(127.0f);
    CHECK(big.to_int() == 127);

    // Largest negative integer representable: -128.x
    s8x24 neg_big(-128.0f);
    CHECK(neg_big.to_int() == -128);

    // Smallest positive fraction
    s8x24 tiny = s8x24::from_raw(1);
    CHECK(tiny.raw() == 1);
    CHECK(tiny.to_int() == 0);
    CHECK(tiny > s8x24());

    // Smallest negative fraction
    s8x24 neg_tiny = s8x24::from_raw(-1);
    CHECK(neg_tiny.raw() == -1);
    CHECK(neg_tiny < s8x24());
}
