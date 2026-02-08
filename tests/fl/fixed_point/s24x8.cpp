#include "fl/fixed_point/s24x8.h"
#include "doctest.h"

using fl::s24x8;

// Helper: check that a s24x8 value is close to a float within tolerance.
static void check_near(s24x8 val, float expected, float tol = 0.01f) {
    // Convert raw back to float for comparison.
    float actual = static_cast<float>(val.raw()) / (1 << s24x8::FRAC_BITS);
    float diff = actual - expected;
    if (diff < 0) diff = -diff;
    CHECK_MESSAGE(diff <= tol,
                  "expected ~", expected, " got ", actual, " (diff=", diff, ")");
}

TEST_CASE("s24x8 - default construction") {
    s24x8 a;
    CHECK(a.raw() == 0);
    CHECK(a.to_int() == 0);
}

TEST_CASE("s24x8 - float construction") {
    s24x8 one(1.0f);
    CHECK(one.raw() == (1 << 8));
    CHECK(one.to_int() == 1);

    s24x8 half(0.5f);
    CHECK(half.raw() == (1 << 7));
    CHECK(half.to_int() == 0);

    s24x8 neg(-1.0f);
    CHECK(neg.raw() == -(1 << 8));
    CHECK(neg.to_int() == -1);

    s24x8 neg_half(-0.5f);
    CHECK(neg_half.to_int() == -1); // floor(-0.5) = -1 via arithmetic shift

    s24x8 big(100.0f);
    CHECK(big.to_int() == 100);

    s24x8 neg_big(-100.0f);
    CHECK(neg_big.to_int() == -100);
}

TEST_CASE("s24x8 - from_raw") {
    s24x8 a = s24x8::from_raw(0x00000100); // 1.0
    CHECK(a.to_int() == 1);

    s24x8 b = s24x8::from_raw(0x00000280); // 2.5
    CHECK(b.to_int() == 2);
    check_near(b, 2.5f);

    s24x8 c = s24x8::from_raw(-1); // smallest negative fraction
    CHECK(c.raw() == -1);
    CHECK(c.to_int() == -1); // arithmetic shift
}

TEST_CASE("s24x8 - addition") {
    s24x8 a(1.0f);
    s24x8 b(2.0f);
    s24x8 c = a + b;
    CHECK(c.to_int() == 3);
    check_near(c, 3.0f);

    // Fractional addition
    s24x8 d(0.25f);
    s24x8 e(0.75f);
    check_near(d + e, 1.0f);

    // Negative addition
    s24x8 f(-3.0f);
    check_near(a + f, -2.0f);

    // Zero identity
    s24x8 zero;
    CHECK((a + zero).raw() == a.raw());
}

TEST_CASE("s24x8 - subtraction") {
    s24x8 a(5.0f);
    s24x8 b(3.0f);
    check_near(a - b, 2.0f);

    // Result crosses zero
    check_near(b - a, -2.0f);

    // Self subtraction
    s24x8 zero;
    CHECK((a - a).raw() == zero.raw());

    // Fractional
    s24x8 c(1.75f);
    s24x8 d(0.25f);
    check_near(c - d, 1.5f);
}

TEST_CASE("s24x8 - unary negation") {
    s24x8 a(3.5f);
    s24x8 neg_a = -a;
    check_near(neg_a, -3.5f);

    // Double negation
    CHECK((-neg_a).raw() == a.raw());

    // Negate zero
    s24x8 zero;
    CHECK((-zero).raw() == 0);
}

TEST_CASE("s24x8 - fixed-point multiply") {
    // 2 * 3 = 6
    s24x8 a(2.0f);
    s24x8 b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    s24x8 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 is identity
    s24x8 one(1.0f);
    CHECK((a * one).raw() == a.raw());

    // Multiply by 0 is zero
    s24x8 zero;
    CHECK((a * zero).raw() == 0);

    // Negative * positive
    s24x8 neg(-2.0f);
    check_near(neg * b, -6.0f);

    // Negative * negative
    check_near(neg * s24x8(-3.0f), 6.0f);

    // Fractional precision
    s24x8 c(1.5f);
    s24x8 d(2.5f);
    check_near(c * d, 3.75f);
}

TEST_CASE("s24x8 - fixed-point divide") {
    // 6 / 3 = 2
    s24x8 a(6.0f);
    s24x8 b(3.0f);
    check_near(a / b, 2.0f);

    // 1 / 2 = 0.5
    s24x8 one(1.0f);
    s24x8 two(2.0f);
    check_near(one / two, 0.5f);

    // 1 / 4 = 0.25
    s24x8 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 is identity
    CHECK((a / one).raw() == a.raw());

    // Negative dividend
    s24x8 neg(-6.0f);
    check_near(neg / b, -2.0f);

    // Negative divisor
    check_near(a / s24x8(-3.0f), -2.0f);

    // Both negative
    check_near(neg / s24x8(-3.0f), 2.0f);

    // Fractional result
    s24x8 three(3.0f);
    check_near(one / three, 0.3333f, 0.01f);
}

TEST_CASE("s24x8 - scalar multiply") {
    s24x8 a(1.5f);

    // fp * scalar
    check_near(a * 2, 3.0f);
    check_near(a * 0, 0.0f);
    check_near(a * -1, -1.5f);
    check_near(a * (int32_t)100, 150.0f);

    // scalar * fp (commutative friend)
    check_near(2 * a, 3.0f);
    check_near(-3 * a, -4.5f);
}

TEST_CASE("s24x8 - right shift") {
    s24x8 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    s24x8 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);

    // Negative shift preserves sign (arithmetic shift)
    s24x8 neg(-4.0f);
    check_near(neg >> 1, -2.0f);
    check_near(neg >> 2, -1.0f);
}

TEST_CASE("s24x8 - comparisons") {
    s24x8 a(1.0f);
    s24x8 b(2.0f);
    s24x8 c(1.0f);
    s24x8 neg(-1.0f);
    s24x8 zero;

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

TEST_CASE("s24x8 - sin") {
    // sin(0) = 0
    s24x8 zero;
    check_near(s24x8::sin(zero), 0.0f, 0.05f);

    // sin(pi/2) ~ 1
    s24x8 half_pi(1.5707963f);
    check_near(s24x8::sin(half_pi), 1.0f, 0.05f);

    // sin(pi) ~ 0
    s24x8 pi(3.1415926f);
    check_near(s24x8::sin(pi), 0.0f, 0.05f);

    // sin(-pi/2) ~ -1
    s24x8 neg_half_pi(-1.5707963f);
    check_near(s24x8::sin(neg_half_pi), -1.0f, 0.05f);
}

TEST_CASE("s24x8 - cos") {
    // cos(0) = 1
    s24x8 zero;
    check_near(s24x8::cos(zero), 1.0f, 0.05f);

    // cos(pi/2) ~ 0
    s24x8 half_pi(1.5707963f);
    check_near(s24x8::cos(half_pi), 0.0f, 0.05f);

    // cos(pi) ~ -1
    s24x8 pi(3.1415926f);
    check_near(s24x8::cos(pi), -1.0f, 0.05f);
}

TEST_CASE("s24x8 - sincos") {
    s24x8 angle(0.7854f); // ~pi/4
    s24x8 s, c;
    s24x8::sincos(angle, s, c);

    // sin(pi/4) ~ cos(pi/4) ~ 0.7071
    check_near(s, 0.7071f, 0.05f);
    check_near(c, 0.7071f, 0.05f);

    // sincos should match individual sin/cos
    CHECK(s.raw() == s24x8::sin(angle).raw());
    CHECK(c.raw() == s24x8::cos(angle).raw());
}

TEST_CASE("s24x8 - sin^2 + cos^2 = 1") {
    // Pythagorean identity at several angles
    float angles[] = {0.0f, 0.5f, 1.0f, 1.5707963f, 2.0f, 3.1415926f, -1.0f};
    for (float ang : angles) {
        s24x8 a(ang);
        s24x8 s = s24x8::sin(a);
        s24x8 c = s24x8::cos(a);
        s24x8 sum = s * s + c * c;
        check_near(sum, 1.0f, 0.06f);
    }
}

TEST_CASE("s24x8 - mixed arithmetic expressions") {
    // (a + b) * c
    s24x8 a(2.0f);
    s24x8 b(3.0f);
    s24x8 c(0.5f);
    check_near((a + b) * c, 2.5f);

    // a * b - c * d
    s24x8 d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // (a / b) * b ~ a  (round-trip)
    check_near((a / b) * b, 2.0f, 0.01f);

    // Chained operations
    s24x8 one(1.0f);
    s24x8 result = (a + b) * c / one;
    check_near(result, 2.5f);
}

TEST_CASE("s24x8 - to_float") {
    s24x8 one(1.0f);
    CHECK(one.to_float() == doctest::Approx(1.0f).epsilon(0.01f));

    s24x8 half(0.5f);
    CHECK(half.to_float() == doctest::Approx(0.5f).epsilon(0.01f));

    s24x8 neg(-2.5f);
    CHECK(neg.to_float() == doctest::Approx(-2.5f).epsilon(0.01f));

    s24x8 zero;
    CHECK(zero.to_float() == doctest::Approx(0.0f));
}

TEST_CASE("s24x8 - sqrt") {
    // sqrt(4) = 2
    check_near(s24x8::sqrt(s24x8(4.0f)), 2.0f, 0.01f);

    // sqrt(1) = 1
    check_near(s24x8::sqrt(s24x8(1.0f)), 1.0f, 0.01f);

    // sqrt(2) ~ 1.4142
    check_near(s24x8::sqrt(s24x8(2.0f)), 1.4142f, 0.05f);

    // sqrt(0) = 0
    CHECK(s24x8::sqrt(s24x8()).raw() == 0);

    // sqrt(negative) = 0
    CHECK(s24x8::sqrt(s24x8(-1.0f)).raw() == 0);

    // sqrt(9) = 3
    check_near(s24x8::sqrt(s24x8(9.0f)), 3.0f, 0.01f);
}

TEST_CASE("s24x8 - rsqrt") {
    // rsqrt(4) = 0.5
    check_near(s24x8::rsqrt(s24x8(4.0f)), 0.5f, 0.05f);

    // rsqrt(1) = 1
    check_near(s24x8::rsqrt(s24x8(1.0f)), 1.0f, 0.05f);

    // rsqrt(0) = 0 (sentinel)
    CHECK(s24x8::rsqrt(s24x8()).raw() == 0);

    // rsqrt(negative) = 0
    CHECK(s24x8::rsqrt(s24x8(-1.0f)).raw() == 0);
}

TEST_CASE("s24x8 - pow") {
    // 2^3 = 8
    check_near(s24x8::pow(s24x8(2.0f), s24x8(3.0f)), 8.0f, 0.1f);

    // 4^0.5 = 2 (sqrt via pow)
    check_near(s24x8::pow(s24x8(4.0f), s24x8(0.5f)), 2.0f, 0.1f);

    // x^0 = 1
    check_near(s24x8::pow(s24x8(5.0f), s24x8(0.0f)), 1.0f, 0.1f);

    // 0^x = 0
    CHECK(s24x8::pow(s24x8(), s24x8(2.0f)).raw() == 0);

    // negative base = 0
    CHECK(s24x8::pow(s24x8(-1.0f), s24x8(2.0f)).raw() == 0);
}

TEST_CASE("s24x8 - sqrt identity: sqrt(x)^2 ~ x") {
    float values[] = {1.0f, 4.0f, 9.0f, 16.0f, 100.0f};
    for (float v : values) {
        s24x8 x(v);
        s24x8 s = s24x8::sqrt(x);
        s24x8 sq = s * s;
        check_near(sq, v, v * 0.05f + 0.1f);
    }
}

TEST_CASE("s24x8 - floor and ceil") {
    check_near(s24x8::floor(s24x8(2.75f)), 2.0f);
    check_near(s24x8::ceil(s24x8(2.75f)), 3.0f);
    check_near(s24x8::floor(s24x8(-1.25f)), -2.0f);
    check_near(s24x8::ceil(s24x8(-1.25f)), -1.0f);
    check_near(s24x8::floor(s24x8(3.0f)), 3.0f);
    check_near(s24x8::ceil(s24x8(3.0f)), 3.0f);
}

TEST_CASE("s24x8 - fract") {
    check_near(s24x8::fract(s24x8(2.75f)), 0.75f);
    CHECK(s24x8::fract(s24x8(1.0f)).raw() == 0);
    check_near(s24x8::fract(s24x8(0.5f)), 0.5f);
}

TEST_CASE("s24x8 - abs and sign") {
    check_near(s24x8::abs(s24x8(3.5f)), 3.5f);
    check_near(s24x8::abs(s24x8(-3.5f)), 3.5f);
    CHECK(s24x8::abs(s24x8()).raw() == 0);
    check_near(s24x8::sign(s24x8(5.0f)), 1.0f);
    check_near(s24x8::sign(s24x8(-5.0f)), -1.0f);
    CHECK(s24x8::sign(s24x8()).raw() == 0);
}

TEST_CASE("s24x8 - mod") {
    check_near(s24x8::mod(s24x8(5.5f), s24x8(2.0f)), 1.5f);
    check_near(s24x8::mod(s24x8(3.0f), s24x8(1.5f)), 0.0f);
    check_near(s24x8::mod(s24x8(1.0f), s24x8(3.0f)), 1.0f);
}

TEST_CASE("s24x8 - inverse trig") {
    check_near(s24x8::atan(s24x8(1.0f)), 0.7854f, 0.05f);
    check_near(s24x8::atan(s24x8(0.0f)), 0.0f, 0.01f);
    check_near(s24x8::atan2(s24x8(1.0f), s24x8(1.0f)), 0.7854f, 0.05f);
    check_near(s24x8::asin(s24x8(0.0f)), 0.0f, 0.01f);
    check_near(s24x8::asin(s24x8(1.0f)), 1.5708f, 0.05f);
    check_near(s24x8::acos(s24x8(1.0f)), 0.0f, 0.01f);
    check_near(s24x8::acos(s24x8(0.0f)), 1.5708f, 0.05f);
}

TEST_CASE("s24x8 - lerp, clamp, step, smoothstep") {
    check_near(s24x8::lerp(s24x8(0.0f), s24x8(10.0f), s24x8(0.5f)), 5.0f, 0.05f);
    check_near(s24x8::lerp(s24x8(1.0f), s24x8(3.0f), s24x8(0.0f)), 1.0f);
    check_near(s24x8::lerp(s24x8(1.0f), s24x8(3.0f), s24x8(1.0f)), 3.0f);

    check_near(s24x8::clamp(s24x8(5.0f), s24x8(0.0f), s24x8(3.0f)), 3.0f);
    check_near(s24x8::clamp(s24x8(-1.0f), s24x8(0.0f), s24x8(3.0f)), 0.0f);
    check_near(s24x8::clamp(s24x8(1.5f), s24x8(0.0f), s24x8(3.0f)), 1.5f);

    check_near(s24x8::step(s24x8(1.0f), s24x8(0.5f)), 0.0f);
    check_near(s24x8::step(s24x8(1.0f), s24x8(1.5f)), 1.0f);
    check_near(s24x8::step(s24x8(1.0f), s24x8(1.0f)), 1.0f);

    check_near(s24x8::smoothstep(s24x8(0.0f), s24x8(1.0f), s24x8(-0.5f)), 0.0f, 0.01f);
    check_near(s24x8::smoothstep(s24x8(0.0f), s24x8(1.0f), s24x8(1.5f)), 1.0f, 0.01f);
    check_near(s24x8::smoothstep(s24x8(0.0f), s24x8(1.0f), s24x8(0.5f)), 0.5f, 0.05f);
}

TEST_CASE("s24x8 - edge values") {
    // Largest positive integer representable: 8388607 (2^23-1)
    s24x8 big(8388607.0f);
    CHECK(big.to_int() == 8388607);

    // Largest negative integer representable: -8388608 (-2^23)
    s24x8 neg_big(-8388608.0f);
    CHECK(neg_big.to_int() == -8388608);

    // Smallest positive fraction
    s24x8 tiny = s24x8::from_raw(1);
    CHECK(tiny.raw() == 1);
    CHECK(tiny.to_int() == 0);
    CHECK(tiny > s24x8());

    // Smallest negative fraction
    s24x8 neg_tiny = s24x8::from_raw(-1);
    CHECK(neg_tiny.raw() == -1);
    CHECK(neg_tiny < s24x8());
}
