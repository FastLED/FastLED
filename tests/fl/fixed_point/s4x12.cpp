#include "fl/fixed_point/s4x12.h"
#include "doctest.h"

using fl::s4x12;

// Helper: check that a s4x12 value is close to a float within tolerance.
static void check_near(s4x12 val, float expected, float tol = 0.001f) {
    // Convert raw back to float for comparison.
    float actual = static_cast<float>(val.raw()) / (1 << s4x12::FRAC_BITS);
    float diff = actual - expected;
    if (diff < 0) diff = -diff;
    CHECK_MESSAGE(diff <= tol,
                  "expected ~", expected, " got ", actual, " (diff=", diff, ")");
}

TEST_CASE("s4x12 - default construction") {
    s4x12 a;
    CHECK(a.raw() == 0);
    CHECK(a.to_int() == 0);
}

TEST_CASE("s4x12 - float construction") {
    s4x12 one(1.0f);
    CHECK(one.raw() == (1 << 12));
    CHECK(one.to_int() == 1);

    s4x12 half(0.5f);
    CHECK(half.raw() == (1 << 11));
    CHECK(half.to_int() == 0);

    s4x12 neg(-1.0f);
    CHECK(neg.raw() == -(1 << 12));
    CHECK(neg.to_int() == -1);

    s4x12 neg_half(-0.5f);
    CHECK(neg_half.to_int() == -1); // floor(-0.5) = -1 via arithmetic shift

    s4x12 big(5.0f);
    CHECK(big.to_int() == 5);

    s4x12 neg_big(-5.0f);
    CHECK(neg_big.to_int() == -5);
}

TEST_CASE("s4x12 - from_raw") {
    s4x12 a = s4x12::from_raw(4096); // 1.0
    CHECK(a.to_int() == 1);

    s4x12 b = s4x12::from_raw(10240); // 2.5
    CHECK(b.to_int() == 2);
    check_near(b, 2.5f);

    s4x12 c = s4x12::from_raw(-1); // smallest negative fraction
    CHECK(c.raw() == -1);
    CHECK(c.to_int() == -1); // arithmetic shift
}

TEST_CASE("s4x12 - addition") {
    s4x12 a(1.0f);
    s4x12 b(2.0f);
    s4x12 c = a + b;
    CHECK(c.to_int() == 3);
    check_near(c, 3.0f);

    // Fractional addition
    s4x12 d(0.25f);
    s4x12 e(0.75f);
    check_near(d + e, 1.0f);

    // Negative addition
    s4x12 f(-3.0f);
    check_near(a + f, -2.0f);

    // Zero identity
    s4x12 zero;
    CHECK((a + zero).raw() == a.raw());
}

TEST_CASE("s4x12 - subtraction") {
    s4x12 a(3.0f);
    s4x12 b(1.0f);
    check_near(a - b, 2.0f);

    // Result crosses zero
    check_near(b - a, -2.0f);

    // Self subtraction
    s4x12 zero;
    CHECK((a - a).raw() == zero.raw());

    // Fractional
    s4x12 c(1.75f);
    s4x12 d(0.25f);
    check_near(c - d, 1.5f);
}

TEST_CASE("s4x12 - unary negation") {
    s4x12 a(3.5f);
    s4x12 neg_a = -a;
    check_near(neg_a, -3.5f);

    // Double negation
    CHECK((-neg_a).raw() == a.raw());

    // Negate zero
    s4x12 zero;
    CHECK((-zero).raw() == 0);
}

TEST_CASE("s4x12 - fixed-point multiply") {
    // 2 * 3 = 6
    s4x12 a(2.0f);
    s4x12 b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    s4x12 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 is identity
    s4x12 one(1.0f);
    CHECK((a * one).raw() == a.raw());

    // Multiply by 0 is zero
    s4x12 zero;
    CHECK((a * zero).raw() == 0);

    // Negative * positive
    s4x12 neg(-2.0f);
    check_near(neg * b, -6.0f);

    // Negative * negative
    check_near(neg * s4x12(-3.0f), 6.0f);

    // Fractional precision
    s4x12 c(1.5f);
    s4x12 d(2.0f);
    check_near(c * d, 3.0f);
}

TEST_CASE("s4x12 - fixed-point divide") {
    // 6 / 3 = 2
    s4x12 a(6.0f);
    s4x12 b(3.0f);
    check_near(a / b, 2.0f);

    // 1 / 2 = 0.5
    s4x12 one(1.0f);
    s4x12 two(2.0f);
    check_near(one / two, 0.5f);

    // 1 / 4 = 0.25
    s4x12 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 is identity
    CHECK((a / one).raw() == a.raw());

    // Negative dividend
    s4x12 neg(-6.0f);
    check_near(neg / b, -2.0f);

    // Negative divisor
    check_near(a / s4x12(-3.0f), -2.0f);

    // Both negative
    check_near(neg / s4x12(-3.0f), 2.0f);

    // Fractional result
    s4x12 three(3.0f);
    check_near(one / three, 0.3333f, 0.001f);
}

TEST_CASE("s4x12 - scalar multiply") {
    s4x12 a(1.5f);

    // fp * scalar
    check_near(a * (int16_t)2, 3.0f);
    check_near(a * (int16_t)0, 0.0f);
    check_near(a * (int16_t)-1, -1.5f);

    // scalar * fp (commutative friend)
    check_near((int16_t)2 * a, 3.0f);
    check_near((int16_t)-3 * a, -4.5f);
}

TEST_CASE("s4x12 - right shift") {
    s4x12 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    s4x12 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);

    // Negative shift preserves sign (arithmetic shift)
    s4x12 neg(-4.0f);
    check_near(neg >> 1, -2.0f);
    check_near(neg >> 2, -1.0f);
}

TEST_CASE("s4x12 - comparisons") {
    s4x12 a(1.0f);
    s4x12 b(2.0f);
    s4x12 c(1.0f);
    s4x12 neg(-1.0f);
    s4x12 zero;

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

TEST_CASE("s4x12 - sin") {
    // sin(0) = 0
    s4x12 zero;
    check_near(s4x12::sin(zero), 0.0f, 0.01f);

    // sin(pi/2) ~ 1
    s4x12 half_pi(1.5707963f);
    check_near(s4x12::sin(half_pi), 1.0f, 0.02f);

    // sin(pi) ~ 0
    s4x12 pi(3.1415926f);
    check_near(s4x12::sin(pi), 0.0f, 0.02f);

    // sin(-pi/2) ~ -1
    s4x12 neg_half_pi(-1.5707963f);
    check_near(s4x12::sin(neg_half_pi), -1.0f, 0.02f);
}

TEST_CASE("s4x12 - cos") {
    // cos(0) = 1
    s4x12 zero;
    check_near(s4x12::cos(zero), 1.0f, 0.01f);

    // cos(pi/2) ~ 0
    s4x12 half_pi(1.5707963f);
    check_near(s4x12::cos(half_pi), 0.0f, 0.02f);

    // cos(pi) ~ -1
    s4x12 pi(3.1415926f);
    check_near(s4x12::cos(pi), -1.0f, 0.02f);
}

TEST_CASE("s4x12 - sincos") {
    s4x12 angle(0.7854f); // ~pi/4
    s4x12 s, c;
    s4x12::sincos(angle, s, c);

    // sin(pi/4) ~ cos(pi/4) ~ 0.7071
    check_near(s, 0.7071f, 0.02f);
    check_near(c, 0.7071f, 0.02f);

    // sincos should match individual sin/cos
    CHECK(s.raw() == s4x12::sin(angle).raw());
    CHECK(c.raw() == s4x12::cos(angle).raw());
}

TEST_CASE("s4x12 - sin^2 + cos^2 = 1") {
    // Pythagorean identity at several angles
    float angles[] = {0.0f, 0.5f, 1.0f, 1.5707963f, 2.0f, 3.1415926f, -1.0f};
    for (float ang : angles) {
        s4x12 a(ang);
        s4x12 s = s4x12::sin(a);
        s4x12 c = s4x12::cos(a);
        s4x12 sum = s * s + c * c;
        check_near(sum, 1.0f, 0.03f);
    }
}

TEST_CASE("s4x12 - mixed arithmetic expressions") {
    // (a + b) * c
    s4x12 a(1.0f);
    s4x12 b(2.0f);
    s4x12 c(0.5f);
    check_near((a + b) * c, 1.5f);

    // a * b - c * d
    s4x12 d(2.0f);
    check_near(a * b - c * d, 1.0f); // 2 - 1 = 1

    // (a / b) * b ~ a  (round-trip)
    check_near((a / b) * b, 1.0f, 0.001f);

    // Chained operations
    s4x12 one(1.0f);
    s4x12 result = (a + b) * c / one;
    check_near(result, 1.5f);
}

TEST_CASE("s4x12 - to_float") {
    s4x12 one(1.0f);
    CHECK(one.to_float() == doctest::Approx(1.0f).epsilon(0.001f));

    s4x12 half(0.5f);
    CHECK(half.to_float() == doctest::Approx(0.5f).epsilon(0.001f));

    s4x12 neg(-2.5f);
    CHECK(neg.to_float() == doctest::Approx(-2.5f).epsilon(0.001f));

    s4x12 zero;
    CHECK(zero.to_float() == doctest::Approx(0.0f));
}

TEST_CASE("s4x12 - sqrt") {
    // sqrt(4) = 2
    check_near(s4x12::sqrt(s4x12(4.0f)), 2.0f, 0.01f);

    // sqrt(1) = 1
    check_near(s4x12::sqrt(s4x12(1.0f)), 1.0f, 0.01f);

    // sqrt(2) ~ 1.4142
    check_near(s4x12::sqrt(s4x12(2.0f)), 1.4142f, 0.01f);

    // sqrt(0) = 0
    CHECK(s4x12::sqrt(s4x12()).raw() == 0);

    // sqrt(negative) = 0
    CHECK(s4x12::sqrt(s4x12(-1.0f)).raw() == 0);
}

TEST_CASE("s4x12 - rsqrt") {
    // rsqrt(4) = 0.5
    check_near(s4x12::rsqrt(s4x12(4.0f)), 0.5f, 0.01f);

    // rsqrt(1) = 1
    check_near(s4x12::rsqrt(s4x12(1.0f)), 1.0f, 0.01f);

    // rsqrt(0) = 0 (sentinel)
    CHECK(s4x12::rsqrt(s4x12()).raw() == 0);

    // rsqrt(negative) = 0
    CHECK(s4x12::rsqrt(s4x12(-1.0f)).raw() == 0);
}

TEST_CASE("s4x12 - pow") {
    // 2^3 = 8 (exceeds s4x12 range of [-8, 7.x], so skip this one)
    // 4^0.5 = 2 (sqrt via pow)
    check_near(s4x12::pow(s4x12(4.0f), s4x12(0.5f)), 2.0f, 0.05f);

    // x^0 = 1
    check_near(s4x12::pow(s4x12(5.0f), s4x12(0.0f)), 1.0f, 0.05f);

    // 2^2 = 4
    check_near(s4x12::pow(s4x12(2.0f), s4x12(2.0f)), 4.0f, 0.05f);

    // 0^x = 0
    CHECK(s4x12::pow(s4x12(), s4x12(2.0f)).raw() == 0);

    // negative base = 0
    CHECK(s4x12::pow(s4x12(-1.0f), s4x12(2.0f)).raw() == 0);
}

TEST_CASE("s4x12 - sqrt identity: sqrt(x)^2 ~ x") {
    float values[] = {1.0f, 2.0f, 4.0f};
    for (float v : values) {
        s4x12 x(v);
        s4x12 s = s4x12::sqrt(x);
        s4x12 sq = s * s;
        check_near(sq, v, v * 0.01f + 0.01f);
    }
}

TEST_CASE("s4x12 - floor and ceil") {
    check_near(s4x12::floor(s4x12(2.75f)), 2.0f);
    check_near(s4x12::ceil(s4x12(2.75f)), 3.0f);
    check_near(s4x12::floor(s4x12(-1.25f)), -2.0f);
    check_near(s4x12::ceil(s4x12(-1.25f)), -1.0f);
    check_near(s4x12::floor(s4x12(3.0f)), 3.0f);
    check_near(s4x12::ceil(s4x12(3.0f)), 3.0f);
}

TEST_CASE("s4x12 - fract") {
    check_near(s4x12::fract(s4x12(2.75f)), 0.75f);
    CHECK(s4x12::fract(s4x12(1.0f)).raw() == 0);
    check_near(s4x12::fract(s4x12(0.5f)), 0.5f);
}

TEST_CASE("s4x12 - abs and sign") {
    check_near(s4x12::abs(s4x12(3.5f)), 3.5f);
    check_near(s4x12::abs(s4x12(-3.5f)), 3.5f);
    CHECK(s4x12::abs(s4x12()).raw() == 0);
    check_near(s4x12::sign(s4x12(5.0f)), 1.0f);
    check_near(s4x12::sign(s4x12(-5.0f)), -1.0f);
    CHECK(s4x12::sign(s4x12()).raw() == 0);
}

TEST_CASE("s4x12 - mod") {
    check_near(s4x12::mod(s4x12(5.5f), s4x12(2.0f)), 1.5f);
    check_near(s4x12::mod(s4x12(3.0f), s4x12(1.5f)), 0.0f);
    check_near(s4x12::mod(s4x12(1.0f), s4x12(3.0f)), 1.0f);
}

TEST_CASE("s4x12 - inverse trig") {
    check_near(s4x12::atan(s4x12(1.0f)), 0.7854f, 0.01f);
    check_near(s4x12::atan(s4x12(0.0f)), 0.0f, 0.001f);
    check_near(s4x12::atan2(s4x12(1.0f), s4x12(1.0f)), 0.7854f, 0.01f);
    check_near(s4x12::asin(s4x12(0.0f)), 0.0f, 0.001f);
    check_near(s4x12::asin(s4x12(1.0f)), 1.5708f, 0.01f);
    check_near(s4x12::acos(s4x12(1.0f)), 0.0f, 0.001f);
    check_near(s4x12::acos(s4x12(0.0f)), 1.5708f, 0.01f);
}

TEST_CASE("s4x12 - lerp, clamp, step, smoothstep") {
    check_near(s4x12::lerp(s4x12(0.0f), s4x12(4.0f), s4x12(0.5f)), 2.0f, 0.01f);
    check_near(s4x12::lerp(s4x12(1.0f), s4x12(3.0f), s4x12(0.0f)), 1.0f);
    check_near(s4x12::lerp(s4x12(1.0f), s4x12(3.0f), s4x12(1.0f)), 3.0f);

    check_near(s4x12::clamp(s4x12(5.0f), s4x12(0.0f), s4x12(3.0f)), 3.0f);
    check_near(s4x12::clamp(s4x12(-1.0f), s4x12(0.0f), s4x12(3.0f)), 0.0f);
    check_near(s4x12::clamp(s4x12(1.5f), s4x12(0.0f), s4x12(3.0f)), 1.5f);

    check_near(s4x12::step(s4x12(1.0f), s4x12(0.5f)), 0.0f);
    check_near(s4x12::step(s4x12(1.0f), s4x12(1.5f)), 1.0f);
    check_near(s4x12::step(s4x12(1.0f), s4x12(1.0f)), 1.0f);

    check_near(s4x12::smoothstep(s4x12(0.0f), s4x12(1.0f), s4x12(-0.5f)), 0.0f, 0.001f);
    check_near(s4x12::smoothstep(s4x12(0.0f), s4x12(1.0f), s4x12(1.5f)), 1.0f, 0.001f);
    check_near(s4x12::smoothstep(s4x12(0.0f), s4x12(1.0f), s4x12(0.5f)), 0.5f, 0.02f);
}

TEST_CASE("s4x12 - edge values") {
    // Largest positive integer representable: 7.x (2^3 - 1)
    s4x12 big(7.0f);
    CHECK(big.to_int() == 7);

    // Largest negative integer representable: -8.x (-2^3)
    s4x12 neg_big(-8.0f);
    CHECK(neg_big.to_int() == -8);

    // Smallest positive fraction
    s4x12 tiny = s4x12::from_raw(1);
    CHECK(tiny.raw() == 1);
    CHECK(tiny.to_int() == 0);
    CHECK(tiny > s4x12());

    // Smallest negative fraction
    s4x12 neg_tiny = s4x12::from_raw(-1);
    CHECK(neg_tiny.raw() == -1);
    CHECK(neg_tiny < s4x12());
}
