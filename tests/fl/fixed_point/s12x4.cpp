#include "fl/fixed_point/s12x4.h"
#include "doctest.h"

using fl::s12x4;

// Helper: check that a s12x4 value is close to a float within tolerance.
static void check_near(s12x4 val, float expected, float tol = 0.1f) {
    // Convert raw back to float for comparison.
    float actual = static_cast<float>(val.raw()) / (1 << s12x4::FRAC_BITS);
    float diff = actual - expected;
    if (diff < 0) diff = -diff;
    CHECK_MESSAGE(diff <= tol,
                  "expected ~", expected, " got ", actual, " (diff=", diff, ")");
}

TEST_CASE("s12x4 - default construction") {
    s12x4 a;
    CHECK(a.raw() == 0);
    CHECK(a.to_int() == 0);
}

TEST_CASE("s12x4 - float construction") {
    s12x4 one(1.0f);
    CHECK(one.raw() == (1 << 4));
    CHECK(one.to_int() == 1);

    s12x4 half(0.5f);
    CHECK(half.raw() == (1 << 3));
    CHECK(half.to_int() == 0);

    s12x4 neg(-1.0f);
    CHECK(neg.raw() == -(1 << 4));
    CHECK(neg.to_int() == -1);

    s12x4 neg_half(-0.5f);
    CHECK(neg_half.to_int() == -1); // floor(-0.5) = -1 via arithmetic shift

    s12x4 big(100.0f);
    CHECK(big.to_int() == 100);

    s12x4 neg_big(-100.0f);
    CHECK(neg_big.to_int() == -100);
}

TEST_CASE("s12x4 - from_raw") {
    s12x4 a = s12x4::from_raw(16); // 1.0
    CHECK(a.to_int() == 1);

    s12x4 b = s12x4::from_raw(40); // 2.5
    CHECK(b.to_int() == 2);
    check_near(b, 2.5f);

    s12x4 c = s12x4::from_raw(-1); // smallest negative fraction
    CHECK(c.raw() == -1);
    CHECK(c.to_int() == -1); // arithmetic shift
}

TEST_CASE("s12x4 - addition") {
    s12x4 a(1.0f);
    s12x4 b(2.0f);
    s12x4 c = a + b;
    CHECK(c.to_int() == 3);
    check_near(c, 3.0f);

    // Fractional addition
    s12x4 d(0.25f);
    s12x4 e(0.75f);
    check_near(d + e, 1.0f);

    // Negative addition
    s12x4 f(-3.0f);
    check_near(a + f, -2.0f);

    // Zero identity
    s12x4 zero;
    CHECK((a + zero).raw() == a.raw());
}

TEST_CASE("s12x4 - subtraction") {
    s12x4 a(5.0f);
    s12x4 b(3.0f);
    check_near(a - b, 2.0f);

    // Result crosses zero
    check_near(b - a, -2.0f);

    // Self subtraction
    s12x4 zero;
    CHECK((a - a).raw() == zero.raw());

    // Fractional
    s12x4 c(1.75f);
    s12x4 d(0.25f);
    check_near(c - d, 1.5f);
}

TEST_CASE("s12x4 - unary negation") {
    s12x4 a(3.5f);
    s12x4 neg_a = -a;
    check_near(neg_a, -3.5f);

    // Double negation
    CHECK((-neg_a).raw() == a.raw());

    // Negate zero
    s12x4 zero;
    CHECK((-zero).raw() == 0);
}

TEST_CASE("s12x4 - fixed-point multiply") {
    // 2 * 3 = 6
    s12x4 a(2.0f);
    s12x4 b(3.0f);
    check_near(a * b, 6.0f);

    // 0.5 * 0.5 = 0.25
    s12x4 half(0.5f);
    check_near(half * half, 0.25f);

    // Multiply by 1 is identity
    s12x4 one(1.0f);
    CHECK((a * one).raw() == a.raw());

    // Multiply by 0 is zero
    s12x4 zero;
    CHECK((a * zero).raw() == 0);

    // Negative * positive
    s12x4 neg(-2.0f);
    check_near(neg * b, -6.0f);

    // Negative * negative
    check_near(neg * s12x4(-3.0f), 6.0f);

    // Fractional precision
    s12x4 c(1.5f);
    s12x4 d(2.5f);
    check_near(c * d, 3.75f);
}

TEST_CASE("s12x4 - fixed-point divide") {
    // 6 / 3 = 2
    s12x4 a(6.0f);
    s12x4 b(3.0f);
    check_near(a / b, 2.0f);

    // 1 / 2 = 0.5
    s12x4 one(1.0f);
    s12x4 two(2.0f);
    check_near(one / two, 0.5f);

    // 1 / 4 = 0.25
    s12x4 four(4.0f);
    check_near(one / four, 0.25f);

    // Divide by 1 is identity
    CHECK((a / one).raw() == a.raw());

    // Negative dividend
    s12x4 neg(-6.0f);
    check_near(neg / b, -2.0f);

    // Negative divisor
    check_near(a / s12x4(-3.0f), -2.0f);

    // Both negative
    check_near(neg / s12x4(-3.0f), 2.0f);
}

TEST_CASE("s12x4 - scalar multiply") {
    s12x4 a(1.5f);

    // fp * scalar
    check_near(a * (int16_t)2, 3.0f);
    check_near(a * (int16_t)0, 0.0f);
    check_near(a * (int16_t)-1, -1.5f);
    check_near(a * (int16_t)100, 150.0f);

    // scalar * fp (commutative friend)
    check_near((int16_t)2 * a, 3.0f);
    check_near((int16_t)-3 * a, -4.5f);
}

TEST_CASE("s12x4 - right shift") {
    s12x4 a(4.0f);
    check_near(a >> 1, 2.0f);
    check_near(a >> 2, 1.0f);

    // Fractional shift
    s12x4 b(1.0f);
    check_near(b >> 1, 0.5f);
    check_near(b >> 2, 0.25f);

    // Negative shift preserves sign (arithmetic shift)
    s12x4 neg(-4.0f);
    check_near(neg >> 1, -2.0f);
    check_near(neg >> 2, -1.0f);
}

TEST_CASE("s12x4 - comparisons") {
    s12x4 a(1.0f);
    s12x4 b(2.0f);
    s12x4 c(1.0f);
    s12x4 neg(-1.0f);
    s12x4 zero;

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

TEST_CASE("s12x4 - sin") {
    // sin(0) = 0
    s12x4 zero;
    check_near(s12x4::sin(zero), 0.0f, 0.15f);

    // sin(pi/2) ~ 1
    s12x4 half_pi(1.5707963f);
    check_near(s12x4::sin(half_pi), 1.0f, 0.15f);

    // sin(pi) ~ 0
    s12x4 pi(3.1415926f);
    check_near(s12x4::sin(pi), 0.0f, 0.15f);

    // sin(-pi/2) ~ -1
    s12x4 neg_half_pi(-1.5707963f);
    check_near(s12x4::sin(neg_half_pi), -1.0f, 0.15f);
}

TEST_CASE("s12x4 - cos") {
    // cos(0) = 1
    s12x4 zero;
    check_near(s12x4::cos(zero), 1.0f, 0.15f);

    // cos(pi/2) ~ 0
    s12x4 half_pi(1.5707963f);
    check_near(s12x4::cos(half_pi), 0.0f, 0.15f);

    // cos(pi) ~ -1
    s12x4 pi(3.1415926f);
    check_near(s12x4::cos(pi), -1.0f, 0.15f);
}

TEST_CASE("s12x4 - sincos") {
    s12x4 angle(0.7854f); // ~pi/4
    s12x4 s, c;
    s12x4::sincos(angle, s, c);

    // sin(pi/4) ~ cos(pi/4) ~ 0.7071
    check_near(s, 0.7071f, 0.15f);
    check_near(c, 0.7071f, 0.15f);

    // sincos should match individual sin/cos
    CHECK(s.raw() == s12x4::sin(angle).raw());
    CHECK(c.raw() == s12x4::cos(angle).raw());
}

TEST_CASE("s12x4 - sin^2 + cos^2 = 1") {
    // Pythagorean identity at several angles
    float angles[] = {0.0f, 0.5f, 1.0f, 1.5707963f, 3.1415926f, -1.0f};
    for (float ang : angles) {
        s12x4 a(ang);
        s12x4 s = s12x4::sin(a);
        s12x4 c = s12x4::cos(a);
        s12x4 sum = s * s + c * c;
        check_near(sum, 1.0f, 0.15f);
    }
}

TEST_CASE("s12x4 - mixed arithmetic expressions") {
    // (a + b) * c
    s12x4 a(2.0f);
    s12x4 b(3.0f);
    s12x4 c(0.5f);
    check_near((a + b) * c, 2.5f);

    // a * b - c * d
    s12x4 d(4.0f);
    check_near(a * b - c * d, 4.0f); // 6 - 2 = 4

    // (a / b) * b ~ a  (round-trip, coarse with only 4 frac bits)
    check_near((a / b) * b, 2.0f, 0.15f);

    // Chained operations
    s12x4 one(1.0f);
    s12x4 result = (a + b) * c / one;
    check_near(result, 2.5f);
}

TEST_CASE("s12x4 - to_float") {
    s12x4 one(1.0f);
    CHECK(one.to_float() == doctest::Approx(1.0f).epsilon(0.1f));

    s12x4 half(0.5f);
    CHECK(half.to_float() == doctest::Approx(0.5f).epsilon(0.1f));

    s12x4 neg(-2.5f);
    CHECK(neg.to_float() == doctest::Approx(-2.5f).epsilon(0.1f));

    s12x4 zero;
    CHECK(zero.to_float() == doctest::Approx(0.0f));
}

TEST_CASE("s12x4 - sqrt") {
    // sqrt(4) = 2
    check_near(s12x4::sqrt(s12x4(4.0f)), 2.0f, 0.15f);

    // sqrt(1) = 1
    check_near(s12x4::sqrt(s12x4(1.0f)), 1.0f, 0.15f);

    // sqrt(9) = 3
    check_near(s12x4::sqrt(s12x4(9.0f)), 3.0f, 0.15f);

    // sqrt(0) = 0
    CHECK(s12x4::sqrt(s12x4()).raw() == 0);

    // sqrt(negative) = 0
    CHECK(s12x4::sqrt(s12x4(-1.0f)).raw() == 0);

    // sqrt(16) = 4
    check_near(s12x4::sqrt(s12x4(16.0f)), 4.0f, 0.15f);
}

TEST_CASE("s12x4 - rsqrt") {
    // rsqrt(4) = 0.5
    check_near(s12x4::rsqrt(s12x4(4.0f)), 0.5f, 0.15f);

    // rsqrt(1) = 1
    check_near(s12x4::rsqrt(s12x4(1.0f)), 1.0f, 0.15f);

    // rsqrt(0) = 0 (sentinel)
    CHECK(s12x4::rsqrt(s12x4()).raw() == 0);

    // rsqrt(negative) = 0
    CHECK(s12x4::rsqrt(s12x4(-1.0f)).raw() == 0);
}

TEST_CASE("s12x4 - pow") {
    // 2^3 = 8
    check_near(s12x4::pow(s12x4(2.0f), s12x4(3.0f)), 8.0f, 0.5f);

    // 4^0.5 = 2 (sqrt via pow)
    check_near(s12x4::pow(s12x4(4.0f), s12x4(0.5f)), 2.0f, 0.5f);

    // x^0 = 1
    check_near(s12x4::pow(s12x4(5.0f), s12x4(0.0f)), 1.0f, 0.5f);

    // 0^x = 0
    CHECK(s12x4::pow(s12x4(), s12x4(2.0f)).raw() == 0);

    // negative base = 0
    CHECK(s12x4::pow(s12x4(-1.0f), s12x4(2.0f)).raw() == 0);
}

TEST_CASE("s12x4 - sqrt identity: sqrt(x)^2 ~ x") {
    float values[] = {1.0f, 4.0f, 9.0f, 16.0f};
    for (float v : values) {
        s12x4 x(v);
        s12x4 s = s12x4::sqrt(x);
        s12x4 sq = s * s;
        check_near(sq, v, v * 0.15f + 0.5f);
    }
}

TEST_CASE("s12x4 - floor and ceil") {
    check_near(s12x4::floor(s12x4(2.75f)), 2.0f);
    check_near(s12x4::ceil(s12x4(2.75f)), 3.0f);
    check_near(s12x4::floor(s12x4(-1.25f)), -2.0f);
    check_near(s12x4::ceil(s12x4(-1.25f)), -1.0f);
    check_near(s12x4::floor(s12x4(3.0f)), 3.0f);
    check_near(s12x4::ceil(s12x4(3.0f)), 3.0f);
}

TEST_CASE("s12x4 - fract") {
    check_near(s12x4::fract(s12x4(2.75f)), 0.75f);
    CHECK(s12x4::fract(s12x4(1.0f)).raw() == 0);
    check_near(s12x4::fract(s12x4(0.5f)), 0.5f);
}

TEST_CASE("s12x4 - abs and sign") {
    check_near(s12x4::abs(s12x4(3.5f)), 3.5f);
    check_near(s12x4::abs(s12x4(-3.5f)), 3.5f);
    CHECK(s12x4::abs(s12x4()).raw() == 0);
    check_near(s12x4::sign(s12x4(5.0f)), 1.0f);
    check_near(s12x4::sign(s12x4(-5.0f)), -1.0f);
    CHECK(s12x4::sign(s12x4()).raw() == 0);
}

TEST_CASE("s12x4 - mod") {
    check_near(s12x4::mod(s12x4(5.5f), s12x4(2.0f)), 1.5f);
    check_near(s12x4::mod(s12x4(3.0f), s12x4(1.5f)), 0.0f);
    check_near(s12x4::mod(s12x4(1.0f), s12x4(3.0f)), 1.0f);
}

TEST_CASE("s12x4 - inverse trig") {
    check_near(s12x4::atan(s12x4(1.0f)), 0.7854f, 0.15f);
    check_near(s12x4::atan(s12x4(0.0f)), 0.0f, 0.15f);
    check_near(s12x4::atan2(s12x4(1.0f), s12x4(1.0f)), 0.7854f, 0.15f);
    check_near(s12x4::asin(s12x4(0.0f)), 0.0f, 0.15f);
    check_near(s12x4::asin(s12x4(1.0f)), 1.5708f, 0.15f);
    check_near(s12x4::acos(s12x4(1.0f)), 0.0f, 0.15f);
    check_near(s12x4::acos(s12x4(0.0f)), 1.5708f, 0.15f);
}

TEST_CASE("s12x4 - lerp, clamp, step, smoothstep") {
    check_near(s12x4::lerp(s12x4(0.0f), s12x4(10.0f), s12x4(0.5f)), 5.0f, 0.15f);
    check_near(s12x4::lerp(s12x4(1.0f), s12x4(3.0f), s12x4(0.0f)), 1.0f);
    check_near(s12x4::lerp(s12x4(1.0f), s12x4(3.0f), s12x4(1.0f)), 3.0f, 0.15f);

    check_near(s12x4::clamp(s12x4(5.0f), s12x4(0.0f), s12x4(3.0f)), 3.0f);
    check_near(s12x4::clamp(s12x4(-1.0f), s12x4(0.0f), s12x4(3.0f)), 0.0f);
    check_near(s12x4::clamp(s12x4(1.5f), s12x4(0.0f), s12x4(3.0f)), 1.5f);

    check_near(s12x4::step(s12x4(1.0f), s12x4(0.5f)), 0.0f);
    check_near(s12x4::step(s12x4(1.0f), s12x4(1.5f)), 1.0f);
    check_near(s12x4::step(s12x4(1.0f), s12x4(1.0f)), 1.0f);

    check_near(s12x4::smoothstep(s12x4(0.0f), s12x4(1.0f), s12x4(-0.5f)), 0.0f, 0.15f);
    check_near(s12x4::smoothstep(s12x4(0.0f), s12x4(1.0f), s12x4(1.5f)), 1.0f, 0.15f);
    check_near(s12x4::smoothstep(s12x4(0.0f), s12x4(1.0f), s12x4(0.5f)), 0.5f, 0.2f);
}

TEST_CASE("s12x4 - edge values") {
    // Largest positive integer representable: 2047.x (2^11 - 1)
    s12x4 big(2047.0f);
    CHECK(big.to_int() == 2047);

    // Largest negative integer representable: -2048.x (-2^11)
    s12x4 neg_big(-2048.0f);
    CHECK(neg_big.to_int() == -2048);

    // Smallest positive fraction
    s12x4 tiny = s12x4::from_raw(1);
    CHECK(tiny.raw() == 1);
    CHECK(tiny.to_int() == 0);
    CHECK(tiny > s12x4());

    // Smallest negative fraction
    s12x4 neg_tiny = s12x4::from_raw(-1);
    CHECK(neg_tiny.raw() == -1);
    CHECK(neg_tiny < s12x4());
}
