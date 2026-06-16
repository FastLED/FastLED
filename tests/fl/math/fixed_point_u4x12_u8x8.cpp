// u4x12 and u8x8 unsigned fixed-point type tests
// Extracted from fixed_point.cpp (sub-issue of #3127).

#include "test.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point.h"
#include "fl/math/math.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace { // Anonymous namespace for u4x12+u8x8 tests

// Scalar/raw type matching raw() return type.
template <typename T>
using raw_t = decltype(T().raw());

// ---------------------------------------------------------------------------
// u4x12 tests (from u4x12.cpp)
// ---------------------------------------------------------------------------

static bool approx_equal(float a, float b, float eps = 0.01f) {
    return (a - b) < eps && (b - a) < eps;
}

FL_TEST_CASE("u4x12: construction from float") {
    u4x12 zero(0.0f);
    FL_CHECK(zero.raw() == 0);
    FL_CHECK(zero.to_float() == 0.0f);

    u4x12 one(1.0f);
    FL_CHECK(one.raw() == 4096);  // 1.0 * 2^12
    FL_CHECK(one.to_float() == 1.0f);

    u4x12 half(0.5f);
    FL_CHECK(half.raw() == 2048);  // 0.5 * 2^12
    FL_CHECK(half.to_float() == 0.5f);

    u4x12 max_val(15.999f);
    FL_CHECK(max_val.to_float() > 15.0f);
    FL_CHECK(max_val.to_float() < 16.0f);

    u4x12 pi(3.14159f);
    FL_CHECK(approx_equal(pi.to_float(), 3.14159f, 0.001f));
}

FL_TEST_CASE("u4x12: from_raw construction") {
    u4x12 zero = u4x12::from_raw(0);
    FL_CHECK(zero.to_float() == 0.0f);

    u4x12 one = u4x12::from_raw(4096);
    FL_CHECK(one.to_float() == 1.0f);

    u4x12 quarter = u4x12::from_raw(1024);
    FL_CHECK(quarter.to_float() == 0.25f);
}

FL_TEST_CASE("u4x12: to_int conversion") {
    u4x12 zero(0.0f);
    FL_CHECK(zero.to_int() == 0);

    u4x12 one(1.0f);
    FL_CHECK(one.to_int() == 1);

    u4x12 two_half(2.5f);
    FL_CHECK(two_half.to_int() == 2);  // Truncates

    u4x12 fifteen(15.0f);
    FL_CHECK(fifteen.to_int() == 15);
}

FL_TEST_CASE("u4x12: addition") {
    u4x12 a(2.0f);
    u4x12 b(3.0f);
    u4x12 c = a + b;
    FL_CHECK(approx_equal(c.to_float(), 5.0f));

    u4x12 d(0.25f);
    u4x12 e(0.75f);
    u4x12 f = d + e;
    FL_CHECK(approx_equal(f.to_float(), 1.0f));
}

FL_TEST_CASE("u4x12: subtraction") {
    u4x12 a(5.0f);
    u4x12 b(3.0f);
    u4x12 c = a - b;
    FL_CHECK(approx_equal(c.to_float(), 2.0f));

    u4x12 d(1.0f);
    u4x12 e(0.25f);
    u4x12 f = d - e;
    FL_CHECK(approx_equal(f.to_float(), 0.75f));
}

FL_TEST_CASE("u4x12: multiplication") {
    u4x12 a(2.0f);
    u4x12 b(3.0f);
    u4x12 c = a * b;
    FL_CHECK(approx_equal(c.to_float(), 6.0f));

    u4x12 d(0.5f);
    u4x12 e(0.5f);
    u4x12 f = d * e;
    FL_CHECK(approx_equal(f.to_float(), 0.25f));

    u4x12 g(4.0f);
    u4x12 h(2.5f);
    u4x12 i = g * h;
    FL_CHECK(approx_equal(i.to_float(), 10.0f));
}

FL_TEST_CASE("u4x12: division") {
    u4x12 a(6.0f);
    u4x12 b(3.0f);
    u4x12 c = a / b;
    FL_CHECK(approx_equal(c.to_float(), 2.0f));

    u4x12 d(1.0f);
    u4x12 e(2.0f);
    u4x12 f = d / e;
    FL_CHECK(approx_equal(f.to_float(), 0.5f));

    u4x12 g(10.0f);
    u4x12 h(4.0f);
    u4x12 i = g / h;
    FL_CHECK(approx_equal(i.to_float(), 2.5f));
}

FL_TEST_CASE("u4x12: scalar multiplication") {
    u4x12 a(2.5f);
    u4x12 b = a * static_cast<u16>(3);
    FL_CHECK(approx_equal(b.to_float(), 7.5f));

    u4x12 c(0.25f);
    u4x12 d = static_cast<u16>(4) * c;
    FL_CHECK(approx_equal(d.to_float(), 1.0f));
}

FL_TEST_CASE("u4x12: shift operations") {
    u4x12 a(8.0f);
    u4x12 b = a >> 1;
    FL_CHECK(approx_equal(b.to_float(), 4.0f));

    u4x12 c(2.0f);
    u4x12 d = c << 1;
    FL_CHECK(approx_equal(d.to_float(), 4.0f));

    u4x12 e(1.0f);
    u4x12 f = e >> 2;
    FL_CHECK(approx_equal(f.to_float(), 0.25f));
}

FL_TEST_CASE("u4x12: comparisons") {
    u4x12 a(3.0f);
    u4x12 b(5.0f);
    u4x12 c(3.0f);

    FL_CHECK(a < b);
    FL_CHECK(b > a);
    FL_CHECK(a <= c);
    FL_CHECK(a >= c);
    FL_CHECK(a == c);
    FL_CHECK(a != b);

    FL_CHECK_FALSE(a > b);
    FL_CHECK_FALSE(b < a);
    FL_CHECK_FALSE(a != c);
}

FL_TEST_CASE("u4x12: mod operation") {
    u4x12 a(7.5f);
    u4x12 b(3.0f);
    u4x12 c = u4x12::mod(a, b);
    FL_CHECK(approx_equal(c.to_float(), 1.5f));

    u4x12 d(10.0f);
    u4x12 e(4.0f);
    u4x12 f = u4x12::mod(d, e);
    FL_CHECK(approx_equal(f.to_float(), 2.0f));
}

FL_TEST_CASE("u4x12: floor operation") {
    u4x12 a(3.7f);
    u4x12 b = u4x12::floor(a);
    FL_CHECK(approx_equal(b.to_float(), 3.0f));

    u4x12 c(5.1f);
    u4x12 d = u4x12::floor(c);
    FL_CHECK(approx_equal(d.to_float(), 5.0f));

    u4x12 e(0.9f);
    u4x12 f = u4x12::floor(e);
    FL_CHECK(approx_equal(f.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: ceil operation") {
    u4x12 a(3.1f);
    u4x12 b = u4x12::ceil(a);
    FL_CHECK(approx_equal(b.to_float(), 4.0f));

    u4x12 c(5.9f);
    u4x12 d = u4x12::ceil(c);
    FL_CHECK(approx_equal(d.to_float(), 6.0f));

    u4x12 e(2.0f);
    u4x12 f = u4x12::ceil(e);
    FL_CHECK(approx_equal(f.to_float(), 2.0f));
}

FL_TEST_CASE("u4x12: fract operation") {
    u4x12 a(3.75f);
    u4x12 b = u4x12::fract(a);
    FL_CHECK(approx_equal(b.to_float(), 0.75f));

    u4x12 c(5.25f);
    u4x12 d = u4x12::fract(c);
    FL_CHECK(approx_equal(d.to_float(), 0.25f));

    u4x12 e(7.0f);
    u4x12 f = u4x12::fract(e);
    FL_CHECK(approx_equal(f.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: abs operation") {
    u4x12 a(3.5f);
    u4x12 b = u4x12::abs(a);
    FL_CHECK(approx_equal(b.to_float(), 3.5f));

    u4x12 c(0.0f);
    u4x12 d = u4x12::abs(c);
    FL_CHECK(approx_equal(d.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: min/max operations") {
    u4x12 a(3.0f);
    u4x12 b(5.0f);

    u4x12 min_val = u4x12::min(a, b);
    FL_CHECK(approx_equal(min_val.to_float(), 3.0f));

    u4x12 max_val = u4x12::max(a, b);
    FL_CHECK(approx_equal(max_val.to_float(), 5.0f));

    u4x12 c(7.0f);
    u4x12 d(7.0f);
    FL_CHECK(approx_equal(u4x12::min(c, d).to_float(), 7.0f));
    FL_CHECK(approx_equal(u4x12::max(c, d).to_float(), 7.0f));
}

FL_TEST_CASE("u4x12: lerp operation") {
    u4x12 a(0.0f);
    u4x12 b(10.0f);
    u4x12 t(0.5f);
    u4x12 c = u4x12::lerp(a, b, t);
    FL_CHECK(approx_equal(c.to_float(), 5.0f));

    u4x12 d(2.0f);
    u4x12 e(8.0f);
    u4x12 f(0.25f);
    u4x12 g = u4x12::lerp(d, e, f);
    FL_CHECK(approx_equal(g.to_float(), 3.5f));
}

FL_TEST_CASE("u4x12: clamp operation") {
    u4x12 lo(2.0f);
    u4x12 hi(8.0f);

    u4x12 a(5.0f);
    u4x12 b = u4x12::clamp(a, lo, hi);
    FL_CHECK(approx_equal(b.to_float(), 5.0f));

    u4x12 c(1.0f);
    u4x12 d = u4x12::clamp(c, lo, hi);
    FL_CHECK(approx_equal(d.to_float(), 2.0f));

    u4x12 e(10.0f);
    u4x12 f = u4x12::clamp(e, lo, hi);
    FL_CHECK(approx_equal(f.to_float(), 8.0f));
}

FL_TEST_CASE("u4x12: step operation") {
    u4x12 edge(5.0f);

    u4x12 a(3.0f);
    u4x12 b = u4x12::step(edge, a);
    FL_CHECK(approx_equal(b.to_float(), 0.0f));

    u4x12 c(7.0f);
    u4x12 d = u4x12::step(edge, c);
    FL_CHECK(approx_equal(d.to_float(), 1.0f));

    u4x12 e(5.0f);
    u4x12 f = u4x12::step(edge, e);
    FL_CHECK(approx_equal(f.to_float(), 1.0f));  // x >= edge
}

FL_TEST_CASE("u4x12: smoothstep operation") {
    u4x12 edge0(0.0f);
    u4x12 edge1(1.0f);

    u4x12 a(0.5f);
    u4x12 b = u4x12::smoothstep(edge0, edge1, a);
    FL_CHECK(approx_equal(b.to_float(), 0.5f, 0.05f));

    u4x12 c(0.0f);
    u4x12 d = u4x12::smoothstep(edge0, edge1, c);
    FL_CHECK(approx_equal(d.to_float(), 0.0f, 0.05f));

    u4x12 e(1.0f);
    u4x12 f = u4x12::smoothstep(edge0, edge1, e);
    FL_CHECK(approx_equal(f.to_float(), 1.0f, 0.05f));
}

FL_TEST_CASE("u4x12: sqrt operation") {
    u4x12 a(4.0f);
    u4x12 b = u4x12::sqrt(a);
    FL_CHECK(approx_equal(b.to_float(), 2.0f, 0.05f));

    u4x12 c(9.0f);
    u4x12 d = u4x12::sqrt(c);
    FL_CHECK(approx_equal(d.to_float(), 3.0f, 0.05f));

    u4x12 e(2.0f);
    u4x12 f = u4x12::sqrt(e);
    FL_CHECK(approx_equal(f.to_float(), 1.414f, 0.05f));

    u4x12 g(0.0f);
    u4x12 h = u4x12::sqrt(g);
    FL_CHECK(approx_equal(h.to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: rsqrt operation") {
    u4x12 a(4.0f);
    u4x12 b = u4x12::rsqrt(a);
    FL_CHECK(approx_equal(b.to_float(), 0.5f, 0.05f));

    u4x12 c(1.0f);
    u4x12 d = u4x12::rsqrt(c);
    FL_CHECK(approx_equal(d.to_float(), 1.0f, 0.05f));

    u4x12 e(0.25f);
    u4x12 f = u4x12::rsqrt(e);
    FL_CHECK(approx_equal(f.to_float(), 2.0f, 0.05f));
}

FL_TEST_CASE("u4x12: pow operation") {
    u4x12 a(2.0f);
    u4x12 b(3.0f);
    u4x12 c = u4x12::pow(a, b);
    FL_CHECK(approx_equal(c.to_float(), 8.0f, 0.1f));

    u4x12 d(3.0f);
    u4x12 e(2.0f);
    u4x12 f = u4x12::pow(d, e);
    FL_CHECK(approx_equal(f.to_float(), 9.0f, 0.1f));

    u4x12 g(4.0f);
    u4x12 h(0.5f);
    u4x12 i = u4x12::pow(g, h);
    FL_CHECK(approx_equal(i.to_float(), 2.0f, 0.1f));
}

FL_TEST_CASE("u4x12: edge cases") {
    // Zero
    u4x12 zero(0.0f);
    FL_CHECK(zero.raw() == 0);
    FL_CHECK(zero.to_float() == 0.0f);
    FL_CHECK(zero.to_int() == 0);

    // Max representable value (just under 16.0)
    u4x12 max_val = u4x12::from_raw(0xFFFF);
    FL_CHECK(max_val.to_float() > 15.99f);
    FL_CHECK(max_val.to_float() < 16.0f);
    FL_CHECK(max_val.to_int() == 15);

    // Small values
    u4x12 tiny(0.000244140625f);  // 1/4096 exactly
    FL_CHECK(tiny.raw() == 1);

    // Operations on zero
    u4x12 one(1.0f);
    FL_CHECK(approx_equal((zero + one).to_float(), 1.0f));
    FL_CHECK(approx_equal((one * zero).to_float(), 0.0f));
    FL_CHECK(approx_equal(u4x12::sqrt(zero).to_float(), 0.0f));
}

FL_TEST_CASE("u4x12: range verification") {
    // Verify we can represent values in [0, 16)
    u4x12 zero(0.0f);
    FL_CHECK(zero.to_float() >= 0.0f);

    u4x12 eight(8.0f);
    FL_CHECK(approx_equal(eight.to_float(), 8.0f));

    u4x12 fifteen(15.0f);
    FL_CHECK(approx_equal(fifteen.to_float(), 15.0f));

    u4x12 almost_sixteen(15.999f);
    FL_CHECK(almost_sixteen.to_float() < 16.0f);
}

FL_TEST_CASE("u4x12: fractional precision") {
    // With 12 fractional bits, we have 1/4096 precision
    float quantum = 1.0f / 4096.0f;

    u4x12 a(quantum);
    FL_CHECK(a.raw() == 1);

    u4x12 b(2.0f * quantum);
    FL_CHECK(b.raw() == 2);

    // Verify we can represent small fractions accurately
    u4x12 quarter(0.25f);
    FL_CHECK(quarter.raw() == 1024);
    FL_CHECK(approx_equal(quarter.to_float(), 0.25f, 0.0001f));

    u4x12 eighth(0.125f);
    FL_CHECK(eighth.raw() == 512);
    FL_CHECK(approx_equal(eighth.to_float(), 0.125f, 0.0001f));
}

FL_TEST_CASE("u4x12: compound operations") {
    // Test (a + b) * c - Keep result under 16
    u4x12 a(1.0f);
    u4x12 b(2.0f);
    u4x12 c(3.0f);
    u4x12 result = (a + b) * c;
    FL_CHECK(approx_equal(result.to_float(), 9.0f));

    // Test (a * b) / c
    u4x12 d(6.0f);
    u4x12 e(2.0f);
    u4x12 f(3.0f);
    u4x12 result2 = (d * e) / f;
    FL_CHECK(approx_equal(result2.to_float(), 4.0f));

    // Test lerp with computed t
    u4x12 g(0.0f);
    u4x12 h(10.0f);
    u4x12 i(1.0f);
    u4x12 j(2.0f);
    u4x12 t = i / j;  // 0.5
    u4x12 result3 = u4x12::lerp(g, h, t);
    FL_CHECK(approx_equal(result3.to_float(), 5.0f));
}


// ---------------------------------------------------------------------------
// u8x8 tests (from u8x8.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("u8x8 - construction from zero") {
    u8x8 zero;
    FL_CHECK(zero.raw() == 0);
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, 0.001f);
}

FL_TEST_CASE("u8x8 - construction from one") {
    u8x8 one(1.0f);
    FL_CHECK(one.raw() == 256);
    FL_CHECK_CLOSE(one.to_float(), 1.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - construction from fractional") {
    u8x8 half(0.5f);
    FL_CHECK(half.raw() == 128);
    FL_CHECK_CLOSE(half.to_float(), 0.5f, 0.01f);
}

FL_TEST_CASE("u8x8 - construction from large value") {
    u8x8 large(100.5f);
    FL_CHECK(large.to_int() == 100);
    FL_CHECK_CLOSE(large.to_float(), 100.5f, 0.01f);
}

FL_TEST_CASE("u8x8 - construction from max value") {
    u8x8 max_val(255.99f);
    FL_CHECK_CLOSE(max_val.to_float(), 255.99f, 0.02f);
}

FL_TEST_CASE("u8x8 - from_raw construction") {
    u8x8 from_raw_test = u8x8::from_raw(512);
    FL_CHECK(from_raw_test.raw() == 512);
    FL_CHECK_CLOSE(from_raw_test.to_float(), 2.0f, 0.01f);
}

// ---- Addition tests ----------------------------------------------------

FL_TEST_CASE("u8x8 - addition basic") {
    u8x8 a(2.5f);
    u8x8 b(1.25f);
    u8x8 c = a + b;
    FL_CHECK_CLOSE(c.to_float(), 3.75f, 0.01f);
}

FL_TEST_CASE("u8x8 - addition large values") {
    u8x8 d(100.0f);
    u8x8 e(50.5f);
    u8x8 f = d + e;
    FL_CHECK_CLOSE(f.to_float(), 150.5f, 0.02f);
}

FL_TEST_CASE("u8x8 - addition overflow") {
    u8x8 big(200.0f);
    u8x8 overflow = big + big;
    // Result wraps in u16 storage
    FL_CHECK(overflow.raw() > 0);
}

// ---- Subtraction tests -------------------------------------------------

FL_TEST_CASE("u8x8 - subtraction basic") {
    u8x8 a(5.5f);
    u8x8 b(2.25f);
    u8x8 c = a - b;
    FL_CHECK_CLOSE(c.to_float(), 3.25f, 0.01f);
}

FL_TEST_CASE("u8x8 - subtraction large values") {
    u8x8 d(100.0f);
    u8x8 e(50.5f);
    u8x8 f = d - e;
    FL_CHECK_CLOSE(f.to_float(), 49.5f, 0.01f);
}

FL_TEST_CASE("u8x8 - subtraction underflow") {
    u8x8 small(1.0f);
    u8x8 large(5.0f);
    u8x8 underflow = small - large;
    // Result wraps negative in u16
    FL_CHECK(underflow.raw() > 0x8000);
}

// ---- Multiplication tests ----------------------------------------------

FL_TEST_CASE("u8x8 - multiplication basic") {
    u8x8 a(2.5f);
    u8x8 b(3.0f);
    u8x8 c = a * b;
    FL_CHECK_CLOSE(c.to_float(), 7.5f, 0.02f);
}

FL_TEST_CASE("u8x8 - multiplication with fraction") {
    u8x8 d(10.0f);
    u8x8 e(0.5f);
    u8x8 f = d * e;
    FL_CHECK_CLOSE(f.to_float(), 5.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - multiplication small values") {
    u8x8 small(0.25f);
    u8x8 g = small * small;
    FL_CHECK_CLOSE(g.to_float(), 0.0625f, 0.005f);
}

FL_TEST_CASE("u8x8 - scalar multiplication") {
    u8x8 h(2.5f);
    u8x8 i = h * 4;
    FL_CHECK_CLOSE(i.to_float(), 10.0f, 0.02f);
}

FL_TEST_CASE("u8x8 - scalar multiplication commutative") {
    u8x8 h(2.5f);
    u8x8 j = 3 * h;
    FL_CHECK_CLOSE(j.to_float(), 7.5f, 0.02f);
}

// ---- Division tests ----------------------------------------------------

FL_TEST_CASE("u8x8 - division basic") {
    u8x8 a(10.0f);
    u8x8 b(2.0f);
    u8x8 c = a / b;
    FL_CHECK_CLOSE(c.to_float(), 5.0f, 0.02f);
}

FL_TEST_CASE("u8x8 - division fractional") {
    u8x8 d(7.5f);
    u8x8 e(2.5f);
    u8x8 f = d / e;
    FL_CHECK_CLOSE(f.to_float(), 3.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - division result less than one") {
    u8x8 g(1.0f);
    u8x8 h(4.0f);
    u8x8 i = g / h;
    FL_CHECK_CLOSE(i.to_float(), 0.25f, 0.01f);
}

FL_TEST_CASE("u8x8 - division by zero") {
    u8x8 j(5.0f);
    u8x8 zero;
    u8x8 k = j / zero;
    FL_CHECK(k.raw() == 0);
}

// ---- Shift tests -------------------------------------------------------

FL_TEST_CASE("u8x8 - right shift") {
    u8x8 a(8.0f);
    u8x8 b = a >> 1;
    FL_CHECK_CLOSE(b.to_float(), 4.0f, 0.01f);

    u8x8 c = a >> 2;
    FL_CHECK_CLOSE(c.to_float(), 2.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - left shift") {
    u8x8 d(4.0f);
    u8x8 e = d << 1;
    FL_CHECK_CLOSE(e.to_float(), 8.0f, 0.01f);

    u8x8 f = d << 2;
    FL_CHECK_CLOSE(f.to_float(), 16.0f, 0.02f);
}

// ---- Comparison tests --------------------------------------------------

FL_TEST_CASE("u8x8 - comparisons") {
    u8x8 a(5.0f);
    u8x8 b(3.0f);
    u8x8 c(5.0f);

    FL_CHECK(a > b);
    FL_CHECK(b < a);
    FL_CHECK(a >= c);
    FL_CHECK(a <= c);
    FL_CHECK(a == c);
    FL_CHECK(a != b);
}

FL_TEST_CASE("u8x8 - comparisons large values") {
    u8x8 d(100.0f);
    u8x8 e(200.0f);
    FL_CHECK(d < e);
    FL_CHECK(e > d);
}

// ---- Math function tests -----------------------------------------------

FL_TEST_CASE("u8x8 - mod") {
    u8x8 a(10.0f);
    u8x8 b(3.0f);
    u8x8 c = u8x8::mod(a, b);
    FL_CHECK_CLOSE(c.to_float(), 1.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - floor") {
    u8x8 d(5.75f);
    u8x8 e = u8x8::floor(d);
    FL_CHECK_CLOSE(e.to_float(), 5.0f, 0.01f);

    u8x8 f(10.25f);
    u8x8 g = u8x8::floor(f);
    FL_CHECK_CLOSE(g.to_float(), 10.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - ceil") {
    u8x8 h(5.25f);
    u8x8 i = u8x8::ceil(h);
    FL_CHECK_CLOSE(i.to_float(), 6.0f, 0.01f);

    u8x8 j(10.0f);
    u8x8 k = u8x8::ceil(j);
    FL_CHECK_CLOSE(k.to_float(), 10.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - fract") {
    u8x8 l(5.75f);
    u8x8 m = u8x8::fract(l);
    FL_CHECK_CLOSE(m.to_float(), 0.75f, 0.01f);
}

FL_TEST_CASE("u8x8 - abs") {
    // Unsigned values are always non-negative
    u8x8 n(5.0f);
    u8x8 o = u8x8::abs(n);
    FL_CHECK_CLOSE(o.to_float(), 5.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - min") {
    u8x8 p(5.0f);
    u8x8 q(3.0f);
    u8x8 r = u8x8::min(p, q);
    FL_CHECK_CLOSE(r.to_float(), 3.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - max") {
    u8x8 p(5.0f);
    u8x8 q(3.0f);
    u8x8 s = u8x8::max(p, q);
    FL_CHECK_CLOSE(s.to_float(), 5.0f, 0.01f);
}

// ---- lerp tests --------------------------------------------------------

FL_TEST_CASE("u8x8 - lerp midpoint") {
    u8x8 a(0.0f);
    u8x8 b(10.0f);
    u8x8 t(0.5f);
    u8x8 c = u8x8::lerp(a, b, t);
    FL_CHECK_CLOSE(c.to_float(), 5.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - lerp at endpoints") {
    u8x8 a(0.0f);
    u8x8 b(10.0f);

    u8x8 t0(0.0f);
    u8x8 d = u8x8::lerp(a, b, t0);
    FL_CHECK_CLOSE(d.to_float(), 0.0f, 0.01f);

    u8x8 t1(1.0f);
    u8x8 e = u8x8::lerp(a, b, t1);
    FL_CHECK_CLOSE(e.to_float(), 10.0f, 0.02f);
}

// ---- clamp tests -------------------------------------------------------

FL_TEST_CASE("u8x8 - clamp within range") {
    u8x8 lo(0.0f);
    u8x8 hi(10.0f);
    u8x8 within(5.0f);
    u8x8 b = u8x8::clamp(within, lo, hi);
    FL_CHECK_CLOSE(b.to_float(), 5.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - clamp above range") {
    u8x8 lo(0.0f);
    u8x8 hi(10.0f);
    u8x8 above(15.0f);
    u8x8 c = u8x8::clamp(above, lo, hi);
    FL_CHECK_CLOSE(c.to_float(), 10.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - clamp below range") {
    u8x8 lo(5.0f);
    u8x8 hi(10.0f);
    u8x8 below(2.0f);
    u8x8 a = u8x8::clamp(below, lo, hi);
    FL_CHECK_CLOSE(a.to_float(), 5.0f, 0.01f);
}

// ---- step tests --------------------------------------------------------

FL_TEST_CASE("u8x8 - step below edge") {
    u8x8 edge(5.0f);
    u8x8 below(3.0f);
    u8x8 a = u8x8::step(edge, below);
    FL_CHECK_CLOSE(a.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - step above edge") {
    u8x8 edge(5.0f);
    u8x8 above(7.0f);
    u8x8 b = u8x8::step(edge, above);
    FL_CHECK_CLOSE(b.to_float(), 1.0f, 0.01f);
}

// ---- smoothstep tests --------------------------------------------------

FL_TEST_CASE("u8x8 - smoothstep below range") {
    u8x8 edge0(0.0f);
    u8x8 edge1(10.0f);
    u8x8 below_val(0.0f);
    u8x8 a = u8x8::smoothstep(edge0, edge1, below_val);
    FL_CHECK_CLOSE(a.to_float(), 0.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - smoothstep midpoint") {
    u8x8 edge0(0.0f);
    u8x8 edge1(10.0f);
    u8x8 mid(5.0f);
    u8x8 b = u8x8::smoothstep(edge0, edge1, mid);
    FL_CHECK_CLOSE(b.to_float(), 0.5f, 0.1f);
}

FL_TEST_CASE("u8x8 - smoothstep above range") {
    u8x8 edge0(0.0f);
    u8x8 edge1(10.0f);
    u8x8 above(15.0f);
    u8x8 c = u8x8::smoothstep(edge0, edge1, above);
    FL_CHECK_CLOSE(c.to_float(), 1.0f, 0.1f);
}

// ---- sqrt tests --------------------------------------------------------

FL_TEST_CASE("u8x8 - sqrt zero") {
    u8x8 zero(0.0f);
    u8x8 a = u8x8::sqrt(zero);
    FL_CHECK_CLOSE(a.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - sqrt perfect squares") {
    u8x8 four(4.0f);
    u8x8 b = u8x8::sqrt(four);
    FL_CHECK_CLOSE(b.to_float(), 2.0f, 0.05f);

    u8x8 nine(9.0f);
    u8x8 c = u8x8::sqrt(nine);
    FL_CHECK_CLOSE(c.to_float(), 3.0f, 0.05f);

    u8x8 hundred(100.0f);
    u8x8 d = u8x8::sqrt(hundred);
    FL_CHECK_CLOSE(d.to_float(), 10.0f, 0.1f);
}

FL_TEST_CASE("u8x8 - sqrt irrational") {
    u8x8 two(2.0f);
    u8x8 e = u8x8::sqrt(two);
    FL_CHECK_CLOSE(e.to_float(), 1.414f, 0.05f);
}

// ---- rsqrt tests -------------------------------------------------------

FL_TEST_CASE("u8x8 - rsqrt") {
    u8x8 four(4.0f);
    u8x8 a = u8x8::rsqrt(four);
    FL_CHECK_CLOSE(a.to_float(), 0.5f, 0.05f);

    u8x8 nine(9.0f);
    u8x8 b = u8x8::rsqrt(nine);
    FL_CHECK_CLOSE(b.to_float(), 0.333f, 0.05f);
}

// ---- pow tests ---------------------------------------------------------

FL_TEST_CASE("u8x8 - pow basic") {
    u8x8 base(2.0f);
    u8x8 exp(3.0f);
    u8x8 a = u8x8::pow(base, exp);
    FL_CHECK_CLOSE(a.to_float(), 8.0f, 0.2f);
}

FL_TEST_CASE("u8x8 - pow larger values") {
    u8x8 base2(10.0f);
    u8x8 exp2(2.0f);
    u8x8 b = u8x8::pow(base2, exp2);
    FL_CHECK_CLOSE(b.to_float(), 100.0f, 2.0f);
}

FL_TEST_CASE("u8x8 - pow zero exponent") {
    u8x8 base3(5.0f);
    u8x8 zero(0.0f);
    u8x8 c = u8x8::pow(base3, zero);
    FL_CHECK_CLOSE(c.to_float(), 1.0f, 0.05f);
}

FL_TEST_CASE("u8x8 - pow one base") {
    u8x8 one(1.0f);
    u8x8 exp3(100.0f);
    u8x8 d = u8x8::pow(one, exp3);
    FL_CHECK_CLOSE(d.to_float(), 1.0f, 0.05f);
}

// ---- Edge case tests ---------------------------------------------------

FL_TEST_CASE("u8x8 - zero operations") {
    u8x8 zero;
    u8x8 one(1.0f);

    u8x8 a = zero + one;
    FL_CHECK_CLOSE(a.to_float(), 1.0f, 0.01f);

    u8x8 b = zero * one;
    FL_CHECK_CLOSE(b.to_float(), 0.0f, 0.01f);
}

FL_TEST_CASE("u8x8 - max value operations") {
    u8x8 max_val(255.0f);
    u8x8 c = u8x8::floor(max_val);
    FL_CHECK_CLOSE(c.to_float(), 255.0f, 1.0f);
}

FL_TEST_CASE("u8x8 - tiny fractional values") {
    u8x8 tiny(0.00390625f);  // 1/256
    FL_CHECK(tiny.raw() > 0);
    FL_CHECK_CLOSE(tiny.to_float(), 0.00390625f, 0.001f);
}

// ---- Type trait tests --------------------------------------------------

FL_TEST_CASE("u8x8 - type traits") {
    FL_CHECK(u8x8::INT_BITS == 8);
    FL_CHECK(u8x8::FRAC_BITS == 8);
}



} // anonymous namespace

} // FL_TEST_FILE
