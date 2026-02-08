#include "fl/fixed_point/s16x16.h"
#include "doctest.h"

using fl::s16x16;

// Helper: check that a s16x16 value is close to a float within tolerance.
static void check_near(s16x16 val, float expected, float tol = 0.001f) {
    // Convert raw back to float for comparison.
    float actual = static_cast<float>(val.raw()) / (1 << s16x16::FRAC_BITS);
    float diff = actual - expected;
    if (diff < 0) diff = -diff;
    CHECK_MESSAGE(diff <= tol,
                  "expected ~", expected, " got ", actual, " (diff=", diff, ")");
}

TEST_CASE("s16x16 - default construction") {
    s16x16 a;
    CHECK(a.raw() == 0);
    CHECK(a.to_int() == 0);
}

TEST_CASE("s16x16 - float construction") {
    s16x16 one(1.0f);
    CHECK(one.raw() == (1 << 16));
    CHECK(one.to_int() == 1);

    s16x16 half(0.5f);
    CHECK(half.raw() == (1 << 15));
    CHECK(half.to_int() == 0);

    s16x16 neg(-1.0f);
    CHECK(neg.raw() == -(1 << 16));
    CHECK(neg.to_int() == -1);

    s16x16 neg_half(-0.5f);
    CHECK(neg_half.to_int() == -1); // floor(-0.5) = -1 via arithmetic shift

    s16x16 big(100.0f);
    CHECK(big.to_int() == 100);

    s16x16 neg_big(-100.0f);
    CHECK(neg_big.to_int() == -100);
}

TEST_CASE("s16x16 - from_raw") {
    s16x16 a = s16x16::from_raw(0x00010000); // 1.0
    CHECK(a.to_int() == 1);

    s16x16 b = s16x16::from_raw(0x00028000); // 2.5
    CHECK(b.to_int() == 2);
    check_near(b, 2.5f);

    s16x16 c = s16x16::from_raw(-1); // smallest negative fraction
    CHECK(c.raw() == -1);
    CHECK(c.to_int() == -1); // arithmetic shift
}

TEST_CASE("s16x16 - addition") {
    s16x16 a(1.0f);
    s16x16 b(2.0f);
    s16x16 c = a + b;
    CHECK(c.to_int() == 3);
    check_near(c, 3.0f);

    // Fractional addition
    s16x16 d(0.25f);
    s16x16 e(0.75f);
    check_near(d + e, 1.0f);

    // Negative addition
    s16x16 f(-3.0f);
    check_near(a + f, -2.0f);

    // Zero identity
    s16x16 zero;
    CHECK((a + zero).raw() == a.raw());
}

TEST_CASE("s16x16 - subtraction") {
    s16x16 a(5.0f);
    s16x16 b(3.0f);
    check_near(a - b, 2.0f);

    // Result crosses zero
    check_near(b - a, -2.0f);

    // Self subtraction
    s16x16 zero;
    CHECK((a - a).raw() == zero.raw());

    // Fractional
    s16x16 c(1.75f);
    s16x16 d(0.25f);
    check_near(c - d, 1.5f);
}

TEST_CASE("s16x16 - unary negation") {
    s16x16 a(3.5f);
    s16x16 neg_a = -a;
    check_near(neg_a, -3.5f);

    // Double negation
    CHECK((-neg_a).raw() == a.raw());

    // Negate zero
    s16x16 zero;
    CHECK((-zero).raw() == 0);
}

TEST_CASE("s16x16 - fixed-point multiply") {
    // 2 * 3 = 6
    s16x16 a(2.0f);
    s16x16 b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    s16x16 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 is identity
    s16x16 one(1.0f);
    CHECK((a * one).raw() == a.raw());

    // Multiply by 0 is zero
    s16x16 zero;
    CHECK((a * zero).raw() == 0);

    // Negative * positive
    s16x16 neg(-2.0f);
    check_near(neg * b, -6.0f);

    // Negative * negative
    check_near(neg * s16x16(-3.0f), 6.0f);

    // Fractional precision
    s16x16 c(1.5f);
    s16x16 d(2.5f);
    check_near(c * d, 3.75f);
}

TEST_CASE("s16x16 - fixed-point divide") {
    // 6 / 3 = 2
    s16x16 a(6.0f);
    s16x16 b(3.0f);
    check_near(a / b, 2.0f);

    // 1 / 2 = 0.5
    s16x16 one(1.0f);
    s16x16 two(2.0f);
    check_near(one / two, 0.5f);

    // 1 / 4 = 0.25
    s16x16 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 is identity
    CHECK((a / one).raw() == a.raw());

    // Negative dividend
    s16x16 neg(-6.0f);
    check_near(neg / b, -2.0f);

    // Negative divisor
    check_near(a / s16x16(-3.0f), -2.0f);

    // Both negative
    check_near(neg / s16x16(-3.0f), 2.0f);

    // Fractional result
    s16x16 three(3.0f);
    check_near(one / three, 0.3333f, 0.001f);
}

TEST_CASE("s16x16 - scalar multiply") {
    s16x16 a(1.5f);

    // fp * scalar
    check_near(a * 2, 3.0f);
    check_near(a * 0, 0.0f);
    check_near(a * -1, -1.5f);
    check_near(a * 100, 150.0f);

    // scalar * fp (commutative friend)
    check_near(2 * a, 3.0f);
    check_near(-3 * a, -4.5f);
}

TEST_CASE("s16x16 - right shift") {
    s16x16 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    s16x16 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);

    // Negative shift preserves sign (arithmetic shift)
    s16x16 neg(-4.0f);
    check_near(neg >> 1, -2.0f);
    check_near(neg >> 2, -1.0f);
}

TEST_CASE("s16x16 - comparisons") {
    s16x16 a(1.0f);
    s16x16 b(2.0f);
    s16x16 c(1.0f);
    s16x16 neg(-1.0f);
    s16x16 zero;

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

TEST_CASE("s16x16 - sin") {
    // sin(0) = 0
    s16x16 zero;
    check_near(s16x16::sin(zero), 0.0f, 0.01f);

    // sin(pi/2) ~ 1
    s16x16 half_pi(1.5707963f);
    check_near(s16x16::sin(half_pi), 1.0f, 0.01f);

    // sin(pi) ~ 0
    s16x16 pi(3.1415926f);
    check_near(s16x16::sin(pi), 0.0f, 0.02f);

    // sin(-pi/2) ~ -1
    s16x16 neg_half_pi(-1.5707963f);
    check_near(s16x16::sin(neg_half_pi), -1.0f, 0.01f);
}

TEST_CASE("s16x16 - cos") {
    // cos(0) = 1
    s16x16 zero;
    check_near(s16x16::cos(zero), 1.0f, 0.01f);

    // cos(pi/2) ~ 0
    s16x16 half_pi(1.5707963f);
    check_near(s16x16::cos(half_pi), 0.0f, 0.02f);

    // cos(pi) ~ -1
    s16x16 pi(3.1415926f);
    check_near(s16x16::cos(pi), -1.0f, 0.01f);
}

TEST_CASE("s16x16 - sincos") {
    s16x16 angle(0.7854f); // ~pi/4
    s16x16 s, c;
    s16x16::sincos(angle, s, c);

    // sin(pi/4) ~ cos(pi/4) ~ 0.7071
    check_near(s, 0.7071f, 0.02f);
    check_near(c, 0.7071f, 0.02f);

    // sincos should match individual sin/cos
    CHECK(s.raw() == s16x16::sin(angle).raw());
    CHECK(c.raw() == s16x16::cos(angle).raw());
}

TEST_CASE("s16x16 - sin^2 + cos^2 = 1") {
    // Pythagorean identity at several angles
    float angles[] = {0.0f, 0.5f, 1.0f, 1.5707963f, 2.0f, 3.1415926f, -1.0f};
    for (float ang : angles) {
        s16x16 a(ang);
        s16x16 s = s16x16::sin(a);
        s16x16 c = s16x16::cos(a);
        s16x16 sum = s * s + c * c;
        check_near(sum, 1.0f, 0.03f);
    }
}

TEST_CASE("s16x16 - mixed arithmetic expressions") {
    // (a + b) * c
    s16x16 a(2.0f);
    s16x16 b(3.0f);
    s16x16 c(0.5f);
    check_near((a + b) * c, 2.5f);

    // a * b - c * d
    s16x16 d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // (a / b) * b ~ a  (round-trip)
    check_near((a / b) * b, 2.0f, 0.001f);

    // Chained operations
    s16x16 one(1.0f);
    s16x16 result = (a + b) * c / one;
    check_near(result, 2.5f);
}

TEST_CASE("s16x16 - to_float") {
    s16x16 one(1.0f);
    CHECK(one.to_float() == doctest::Approx(1.0f).epsilon(0.001f));

    s16x16 half(0.5f);
    CHECK(half.to_float() == doctest::Approx(0.5f).epsilon(0.001f));

    s16x16 neg(-2.5f);
    CHECK(neg.to_float() == doctest::Approx(-2.5f).epsilon(0.001f));

    s16x16 zero;
    CHECK(zero.to_float() == doctest::Approx(0.0f));
}

TEST_CASE("s16x16 - sqrt") {
    // sqrt(4) = 2
    check_near(s16x16::sqrt(s16x16(4.0f)), 2.0f, 0.001f);

    // sqrt(1) = 1
    check_near(s16x16::sqrt(s16x16(1.0f)), 1.0f, 0.001f);

    // sqrt(2) ~ 1.4142
    check_near(s16x16::sqrt(s16x16(2.0f)), 1.4142f, 0.01f);

    // sqrt(0) = 0
    CHECK(s16x16::sqrt(s16x16()).raw() == 0);

    // sqrt(negative) = 0
    CHECK(s16x16::sqrt(s16x16(-1.0f)).raw() == 0);

    // sqrt(9) = 3
    check_near(s16x16::sqrt(s16x16(9.0f)), 3.0f, 0.001f);
}

TEST_CASE("s16x16 - rsqrt") {
    // rsqrt(4) = 0.5
    check_near(s16x16::rsqrt(s16x16(4.0f)), 0.5f, 0.01f);

    // rsqrt(1) = 1
    check_near(s16x16::rsqrt(s16x16(1.0f)), 1.0f, 0.01f);

    // rsqrt(0) = 0 (sentinel)
    CHECK(s16x16::rsqrt(s16x16()).raw() == 0);

    // rsqrt(negative) = 0
    CHECK(s16x16::rsqrt(s16x16(-1.0f)).raw() == 0);
}

TEST_CASE("s16x16 - pow") {
    // 2^3 = 8
    check_near(s16x16::pow(s16x16(2.0f), s16x16(3.0f)), 8.0f, 0.05f);

    // 4^0.5 = 2 (sqrt via pow)
    check_near(s16x16::pow(s16x16(4.0f), s16x16(0.5f)), 2.0f, 0.05f);

    // x^0 = 1
    check_near(s16x16::pow(s16x16(5.0f), s16x16(0.0f)), 1.0f, 0.05f);

    // 0^x = 0
    CHECK(s16x16::pow(s16x16(), s16x16(2.0f)).raw() == 0);

    // negative base = 0
    CHECK(s16x16::pow(s16x16(-1.0f), s16x16(2.0f)).raw() == 0);
}

TEST_CASE("s16x16 - sqrt identity: sqrt(x)^2 ~ x") {
    float values[] = {1.0f, 2.0f, 4.0f, 9.0f, 0.25f, 100.0f};
    for (float v : values) {
        s16x16 x(v);
        s16x16 s = s16x16::sqrt(x);
        s16x16 sq = s * s;
        check_near(sq, v, v * 0.02f + 0.01f);
    }
}

TEST_CASE("s16x16 - floor and ceil") {
    // floor(2.75) = 2
    check_near(s16x16::floor(s16x16(2.75f)), 2.0f);
    // ceil(2.75) = 3
    check_near(s16x16::ceil(s16x16(2.75f)), 3.0f);
    // floor(-1.25) = -2 (two's complement bitmask floors correctly)
    check_near(s16x16::floor(s16x16(-1.25f)), -2.0f);
    // ceil(-1.25) = -1
    check_near(s16x16::ceil(s16x16(-1.25f)), -1.0f);
    // floor(3.0) = 3 (integer no-op)
    check_near(s16x16::floor(s16x16(3.0f)), 3.0f);
    // ceil(3.0) = 3 (integer no-op)
    check_near(s16x16::ceil(s16x16(3.0f)), 3.0f);
}

TEST_CASE("s16x16 - fract") {
    // fract(2.75) = 0.75
    check_near(s16x16::fract(s16x16(2.75f)), 0.75f);
    // fract(1.0) = 0
    CHECK(s16x16::fract(s16x16(1.0f)).raw() == 0);
    // fract(0.5) = 0.5
    check_near(s16x16::fract(s16x16(0.5f)), 0.5f);
}

TEST_CASE("s16x16 - abs and sign") {
    // abs
    check_near(s16x16::abs(s16x16(3.5f)), 3.5f);
    check_near(s16x16::abs(s16x16(-3.5f)), 3.5f);
    CHECK(s16x16::abs(s16x16()).raw() == 0);
    // sign
    check_near(s16x16::sign(s16x16(5.0f)), 1.0f);
    check_near(s16x16::sign(s16x16(-5.0f)), -1.0f);
    CHECK(s16x16::sign(s16x16()).raw() == 0);
}

TEST_CASE("s16x16 - mod") {
    // 5.5 mod 2.0 = 1.5
    check_near(s16x16::mod(s16x16(5.5f), s16x16(2.0f)), 1.5f);
    // 3.0 mod 1.5 = 0
    check_near(s16x16::mod(s16x16(3.0f), s16x16(1.5f)), 0.0f);
    // 1.0 mod 3.0 = 1.0
    check_near(s16x16::mod(s16x16(1.0f), s16x16(3.0f)), 1.0f);
}

TEST_CASE("s16x16 - inverse trig") {
    // atan(1) ~ pi/4
    check_near(s16x16::atan(s16x16(1.0f)), 0.7854f, 0.02f);
    // atan(0) = 0
    check_near(s16x16::atan(s16x16(0.0f)), 0.0f, 0.01f);
    // atan2(1, 1) ~ pi/4
    check_near(s16x16::atan2(s16x16(1.0f), s16x16(1.0f)), 0.7854f, 0.02f);
    // asin(0) = 0
    check_near(s16x16::asin(s16x16(0.0f)), 0.0f, 0.01f);
    // asin(1) ~ pi/2
    check_near(s16x16::asin(s16x16(1.0f)), 1.5708f, 0.02f);
    // acos(1) = 0
    check_near(s16x16::acos(s16x16(1.0f)), 0.0f, 0.01f);
    // acos(0) ~ pi/2
    check_near(s16x16::acos(s16x16(0.0f)), 1.5708f, 0.02f);
}

TEST_CASE("s16x16 - lerp, clamp, step, smoothstep") {
    // lerp(0, 10, 0.5) = 5
    check_near(s16x16::lerp(s16x16(0.0f), s16x16(10.0f), s16x16(0.5f)), 5.0f, 0.01f);
    // lerp endpoints
    check_near(s16x16::lerp(s16x16(1.0f), s16x16(3.0f), s16x16(0.0f)), 1.0f);
    check_near(s16x16::lerp(s16x16(1.0f), s16x16(3.0f), s16x16(1.0f)), 3.0f);

    // clamp
    check_near(s16x16::clamp(s16x16(5.0f), s16x16(0.0f), s16x16(3.0f)), 3.0f);
    check_near(s16x16::clamp(s16x16(-1.0f), s16x16(0.0f), s16x16(3.0f)), 0.0f);
    check_near(s16x16::clamp(s16x16(1.5f), s16x16(0.0f), s16x16(3.0f)), 1.5f);

    // step
    check_near(s16x16::step(s16x16(1.0f), s16x16(0.5f)), 0.0f);
    check_near(s16x16::step(s16x16(1.0f), s16x16(1.5f)), 1.0f);
    check_near(s16x16::step(s16x16(1.0f), s16x16(1.0f)), 1.0f);

    // smoothstep
    check_near(s16x16::smoothstep(s16x16(0.0f), s16x16(1.0f), s16x16(-0.5f)), 0.0f, 0.01f);
    check_near(s16x16::smoothstep(s16x16(0.0f), s16x16(1.0f), s16x16(1.5f)), 1.0f, 0.01f);
    check_near(s16x16::smoothstep(s16x16(0.0f), s16x16(1.0f), s16x16(0.5f)), 0.5f, 0.02f);
}

TEST_CASE("s16x16 - edge values") {
    // Largest positive integer representable: 32767.x
    s16x16 big(32767.0f);
    CHECK(big.to_int() == 32767);

    // Largest negative integer representable: -32768.x
    s16x16 neg_big(-32768.0f);
    CHECK(neg_big.to_int() == -32768);

    // Smallest positive fraction
    s16x16 tiny = s16x16::from_raw(1);
    CHECK(tiny.raw() == 1);
    CHECK(tiny.to_int() == 0);
    CHECK(tiny > s16x16());

    // Smallest negative fraction
    s16x16 neg_tiny = s16x16::from_raw(-1);
    CHECK(neg_tiny.raw() == -1);
    CHECK(neg_tiny < s16x16());
}
