#include "fl/math/math.h"
#include "fl/stl/chrono.h"
#include "fl/stl/int.h"
#include "test.h"
#include <cmath>  // ok include - libm is the reference for the fl::exp accuracy tests

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("fl::floor") {
    FL_SUBCASE("float version") {
        FL_CHECK_EQ(fl::floorf(3.7f), 3.0f);
        FL_CHECK_EQ(fl::floorf(3.0f), 3.0f);
        FL_CHECK_EQ(fl::floorf(-3.7f), -4.0f);
        FL_CHECK_EQ(fl::floorf(-3.0f), -3.0f);
        FL_CHECK_EQ(fl::floorf(0.0f), 0.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK_EQ(fl::floor(3.7), 3.0);
        FL_CHECK_EQ(fl::floor(3.0), 3.0);
        FL_CHECK_EQ(fl::floor(-3.7), -4.0);
        FL_CHECK_EQ(fl::floor(-3.0), -3.0);
        FL_CHECK_EQ(fl::floor(0.0), 0.0);
    }
}

FL_TEST_CASE("fl::ceil") {
    FL_SUBCASE("float version") {
        FL_CHECK_EQ(fl::ceilf(3.2f), 4.0f);
        FL_CHECK_EQ(fl::ceilf(3.0f), 3.0f);
        FL_CHECK_EQ(fl::ceilf(-3.2f), -3.0f);
        FL_CHECK_EQ(fl::ceilf(-3.0f), -3.0f);
        FL_CHECK_EQ(fl::ceilf(0.0f), 0.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK_EQ(fl::ceil(3.2), 4.0);
        FL_CHECK_EQ(fl::ceil(3.0), 3.0);
        FL_CHECK_EQ(fl::ceil(-3.2), -3.0);
        FL_CHECK_EQ(fl::ceil(-3.0), -3.0);
        FL_CHECK_EQ(fl::ceil(0.0), 0.0);
    }

    FL_SUBCASE("constexpr version") {
        constexpr int result1 = fl::ceil_constexpr(3.2f);
        constexpr int result2 = fl::ceil_constexpr(3.0f);
        constexpr int result3 = fl::ceil_constexpr(-3.2f);
        FL_CHECK_EQ(result1, 4);
        FL_CHECK_EQ(result2, 3);
        FL_CHECK_EQ(result3, -3);
    }
}

FL_TEST_CASE("fl::sqrt") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::sqrtf(4.0f)).epsilon(1e-5) == 2.0f);
        FL_CHECK(doctest::Approx(fl::sqrtf(9.0f)).epsilon(1e-5) == 3.0f);
        FL_CHECK(doctest::Approx(fl::sqrtf(2.0f)).epsilon(1e-5) == 1.414213562f);
        FL_CHECK_EQ(fl::sqrtf(0.0f), 0.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::sqrt(4.0)).epsilon(1e-10) == 2.0);
        FL_CHECK(doctest::Approx(fl::sqrt(9.0)).epsilon(1e-10) == 3.0);
        FL_CHECK(doctest::Approx(fl::sqrt(2.0)).epsilon(1e-10) == 1.41421356237);
        FL_CHECK_EQ(fl::sqrt(0.0), 0.0);
    }
}

FL_TEST_CASE("fl::exp") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::expf(0.0f)).epsilon(0.001f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::expf(1.0f)).epsilon(0.001f) == 2.71828f);
        FL_CHECK(doctest::Approx(fl::expf(2.0f)).epsilon(0.001f) == 7.38906f);
        FL_CHECK(doctest::Approx(fl::expf(-1.0f)).epsilon(0.001f) == 0.36788f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::exp(0.0)).epsilon(0.001) == 1.0);
        FL_CHECK(doctest::Approx(fl::exp(1.0)).epsilon(0.001) == 2.71828182845);
        FL_CHECK(doctest::Approx(fl::exp(2.0)).epsilon(0.001) == 7.38905609893);
        FL_CHECK(doctest::Approx(fl::exp(-1.0)).epsilon(0.001) == 0.36787944117);
    }
}

FL_TEST_CASE("fl::sin") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::sinf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::sinf(FL_PI / 2.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::sinf(FL_PI)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::sinf(-FL_PI / 2.0f)).epsilon(1e-6f) == -1.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::sin(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::sin(FL_PI / 2.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::sin(FL_PI)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::sin(-FL_PI / 2.0)).epsilon(1e-10) == -1.0);
    }
}

FL_TEST_CASE("fl::cos") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::cosf(0.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::cosf(FL_PI / 2.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::cosf(FL_PI)).epsilon(1e-6f) == -1.0f);
        FL_CHECK(doctest::Approx(fl::cosf(-FL_PI)).epsilon(1e-6f) == -1.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::cos(0.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::cos(FL_PI / 2.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::cos(FL_PI)).epsilon(1e-10) == -1.0);
        FL_CHECK(doctest::Approx(fl::cos(-FL_PI)).epsilon(1e-10) == -1.0);
    }
}

FL_TEST_CASE("fl::tan") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::tanf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::tanf(FL_PI / 4.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::tanf(-FL_PI / 4.0f)).epsilon(1e-6f) == -1.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::tan(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::tan(FL_PI / 4.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::tan(-FL_PI / 4.0)).epsilon(1e-10) == -1.0);
    }
}

FL_TEST_CASE("fl::log") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::logf(1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::logf(2.71828f)).epsilon(0.001f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::logf(7.38906f)).epsilon(0.001f) == 2.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::log(1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::log(2.71828182845)).epsilon(0.001) == 1.0);
        FL_CHECK(doctest::Approx(fl::log(7.38905609893)).epsilon(0.001) == 2.0);
    }
}

FL_TEST_CASE("fl::log10") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::log10f(1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::log10f(10.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::log10f(100.0f)).epsilon(1e-6f) == 2.0f);
        FL_CHECK(doctest::Approx(fl::log10f(1000.0f)).epsilon(1e-6f) == 3.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::log10(1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::log10(10.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::log10(100.0)).epsilon(1e-10) == 2.0);
        FL_CHECK(doctest::Approx(fl::log10(1000.0)).epsilon(1e-10) == 3.0);
    }
}

FL_TEST_CASE("fl::pow") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 0.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 1.0f)).epsilon(1e-6f) == 2.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 2.0f)).epsilon(1e-6f) == 4.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 3.0f)).epsilon(1e-6f) == 8.0f);
        FL_CHECK(doctest::Approx(fl::powf(3.0f, 2.0f)).epsilon(1e-6f) == 9.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, -1.0f)).epsilon(1e-6f) == 0.5f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::pow(2.0, 0.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, 1.0)).epsilon(1e-10) == 2.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, 2.0)).epsilon(1e-10) == 4.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, 3.0)).epsilon(1e-10) == 8.0);
        FL_CHECK(doctest::Approx(fl::pow(3.0, 2.0)).epsilon(1e-10) == 9.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, -1.0)).epsilon(1e-10) == 0.5);
    }
}

FL_TEST_CASE("fl::fabs") {
    FL_SUBCASE("float version") {
        FL_CHECK_EQ(fl::fabsf(3.5f), 3.5f);
        FL_CHECK_EQ(fl::fabsf(-3.5f), 3.5f);
        FL_CHECK_EQ(fl::fabsf(0.0f), 0.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK_EQ(fl::fabs(3.5), 3.5);
        FL_CHECK_EQ(fl::fabs(-3.5), 3.5);
        FL_CHECK_EQ(fl::fabs(0.0), 0.0);
    }
}

FL_TEST_CASE("fl::lround") {
    FL_SUBCASE("float version") {
        FL_CHECK_EQ(fl::lroundf(3.5f), 4);
        FL_CHECK_EQ(fl::lroundf(3.4f), 3);
        FL_CHECK_EQ(fl::lroundf(-3.5f), -4);
        FL_CHECK_EQ(fl::lroundf(-3.4f), -3);
        FL_CHECK_EQ(fl::lroundf(0.0f), 0);
    }

    FL_SUBCASE("double version") {
        FL_CHECK_EQ(fl::lround(3.5), 4);
        FL_CHECK_EQ(fl::lround(3.4), 3);
        FL_CHECK_EQ(fl::lround(-3.5), -4);
        FL_CHECK_EQ(fl::lround(-3.4), -3);
        FL_CHECK_EQ(fl::lround(0.0), 0);
    }
}

FL_TEST_CASE("fl::fmod") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::fmodf(5.0f, 2.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::fmodf(6.0f, 3.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::fmodf(7.5f, 2.5f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::fmodf(-5.0f, 2.0f)).epsilon(1e-6f) == -1.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::fmod(5.0, 2.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::fmod(6.0, 3.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::fmod(7.5, 2.5)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::fmod(-5.0, 2.0)).epsilon(1e-10) == -1.0);
    }
}

FL_TEST_CASE("fl::atan2") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::atan2f(0.0f, 1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::atan2f(1.0f, 0.0f)).epsilon(1e-6f) == FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::atan2f(0.0f, -1.0f)).epsilon(1e-6f) == FL_PI);
        FL_CHECK(doctest::Approx(fl::atan2f(-1.0f, 0.0f)).epsilon(1e-6f) == -FL_PI / 2.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::atan2(0.0, 1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::atan2(1.0, 0.0)).epsilon(1e-10) == FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::atan2(0.0, -1.0)).epsilon(1e-10) == FL_PI);
        FL_CHECK(doctest::Approx(fl::atan2(-1.0, 0.0)).epsilon(1e-10) == -FL_PI / 2.0);
    }
}

FL_TEST_CASE("fl::hypot") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::hypotf(3.0f, 4.0f)).epsilon(1e-6f) == 5.0f);
        FL_CHECK(doctest::Approx(fl::hypotf(0.0f, 0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::hypotf(1.0f, 1.0f)).epsilon(0.001f) == 1.41421f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::hypot(3.0, 4.0)).epsilon(1e-10) == 5.0);
        FL_CHECK(doctest::Approx(fl::hypot(0.0, 0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::hypot(1.0, 1.0)).epsilon(0.001) == 1.41421356237);
    }
}

FL_TEST_CASE("fl::atan") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::atanf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::atanf(1.0f)).epsilon(1e-6f) == FL_PI / 4.0f);
        FL_CHECK(doctest::Approx(fl::atanf(-1.0f)).epsilon(1e-6f) == -FL_PI / 4.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::atan(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::atan(1.0)).epsilon(1e-10) == FL_PI / 4.0);
        FL_CHECK(doctest::Approx(fl::atan(-1.0)).epsilon(1e-10) == -FL_PI / 4.0);
    }
}

FL_TEST_CASE("fl::asin") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::asinf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::asinf(1.0f)).epsilon(1e-6f) == FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::asinf(-1.0f)).epsilon(1e-6f) == -FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::asinf(0.5f)).epsilon(1e-6f) == FL_PI / 6.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::asin(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::asin(1.0)).epsilon(1e-10) == FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::asin(-1.0)).epsilon(1e-10) == -FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::asin(0.5)).epsilon(1e-10) == FL_PI / 6.0);
    }
}

FL_TEST_CASE("fl::acos") {
    FL_SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::acosf(1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::acosf(0.0f)).epsilon(1e-6f) == FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::acosf(-1.0f)).epsilon(1e-6f) == FL_PI);
        FL_CHECK(doctest::Approx(fl::acosf(0.5f)).epsilon(1e-6f) == FL_PI / 3.0f);
    }

    FL_SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::acos(1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::acos(0.0)).epsilon(1e-10) == FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::acos(-1.0)).epsilon(1e-10) == FL_PI);
        FL_CHECK(doctest::Approx(fl::acos(0.5)).epsilon(1e-10) == FL_PI / 3.0);
    }
}

// Integer arguments should promote to float and return float (not truncate)
FL_TEST_CASE("fl::math integer promotion") {
    FL_SUBCASE("sin/cos with int") {
        float s = fl::sin(1);  // sin(1 radian) ≈ 0.8415
        float c = fl::cos(1);  // cos(1 radian) ≈ 0.5403
        FL_CHECK(doctest::Approx(s).epsilon(0.001f) == 0.8415f);
        FL_CHECK(doctest::Approx(c).epsilon(0.001f) == 0.5403f);
    }

    FL_SUBCASE("sqrt with int") {
        float r = fl::sqrt(4);
        FL_CHECK(doctest::Approx(r).epsilon(1e-5f) == 2.0f);
    }

    FL_SUBCASE("exp with int") {
        float e = fl::exp(1);
        FL_CHECK(doctest::Approx(e).epsilon(0.001f) == 2.71828f);
    }

    FL_SUBCASE("log with int") {
        float l = fl::log(1);
        FL_CHECK(doctest::Approx(l).epsilon(1e-6f) == 0.0f);
    }

    FL_SUBCASE("pow with int") {
        float p = fl::pow(2, 3);
        FL_CHECK(doctest::Approx(p).epsilon(0.001f) == 8.0f);
    }

    FL_SUBCASE("floor/ceil with int") {
        float f = fl::floor(5);
        float c = fl::ceil(5);
        FL_CHECK(f == 5.0f);
        FL_CHECK(c == 5.0f);
    }

    FL_SUBCASE("min/max/abs/clamp with int") {
        FL_CHECK_EQ(fl::min(3, 7), 3);
        FL_CHECK_EQ(fl::max(3, 7), 7);
        FL_CHECK_EQ(fl::abs(-5), 5);
        FL_CHECK_EQ(fl::clamp(15, 0, 10), 10);
    }
}

FL_TEST_CASE("math functions") {
    FL_SUBCASE("fl::min and fl::max") {
        FL_CHECK_EQ(fl::min(5, 10), 5);
        FL_CHECK_EQ(fl::max(5, 10), 10);
        FL_CHECK_EQ(fl::min(-5, -10), -10);
        FL_CHECK_EQ(fl::max(-5, -10), -5);
    }

    FL_SUBCASE("fl::abs") {
        FL_CHECK_EQ(fl::abs(5), 5);
        FL_CHECK_EQ(fl::abs(-5), 5);
        FL_CHECK_EQ(fl::abs(0), 0);
    }

    FL_SUBCASE("fl::almost_equal") {
        FL_CHECK(fl::almost_equal(1.0f, 1.00001f, 0.001f));
        FL_CHECK_FALSE(fl::almost_equal(1.0f, 1.01f, 0.001f));
    }

    FL_SUBCASE("fl::almost_equal float") {
        FL_CHECK(fl::almost_equal(1.0f, 1.0f + FL_EPSILON_F / 2.0f));
    }

    FL_SUBCASE("fl::almost_equal double") {
        FL_CHECK(fl::almost_equal(1.0, 1.0 + FL_EPSILON_D / 2.0));
    }

    FL_SUBCASE("FL_PI") {
        FL_CHECK(doctest::Approx(static_cast<double>(FL_PI)).epsilon(1e-10) == 3.141592653589793);
    }
}


// ---------------------------------------------------------------------------
// fl::exp accuracy and speed (#4288). fl::exp is a range-reduced polynomial on
// every target, host included, so these exercise exactly the code embedded
// targets run; libm is the reference only.
// ---------------------------------------------------------------------------

namespace {

// Distance between two floats in units of the last place of the reference.
double ulp_distance(float got, float want) {
    if (got == want) return 0.0;
    const float spacing = ::nextafterf(want, INFINITY) - want;
    return ::fabs(static_cast<double>(got) - static_cast<double>(want)) / spacing;
}

double ulp_distance(double got, double want) {
    if (got == want) return 0.0;
    const double spacing = ::nextafter(want, INFINITY) - want;
    return ::fabs(got - want) / spacing;
}

float libm_expf(float x) { return ::expf(x); }
float fl_exp_double_as_float(float x) { return static_cast<float>(fl::exp(static_cast<double>(x))); }

typedef float (*ExpFn)(float);
double ns_per_call(ExpFn fn, const float* in, int n, volatile float* sink) {
    float acc = 0.0f;
    const fl::u32 t0 = fl::micros();
    for (int i = 0; i < n; ++i) acc += fn(in[i & 1023]);
    const fl::u32 t1 = fl::micros();
    *sink = *sink + acc;
    return 1000.0 * static_cast<double>(t1 - t0) / n;
}

}  // namespace

FL_TEST_CASE("fl::exp float matches libm to 2 ulp across the float range") {
    double worst = 0.0;
    float worst_x = 0.0f;
    // 1e-3 steps over the whole finite range: ~190k points.
    for (float x = -103.9f; x < 88.7f; x += 0.001f) {
        const double d = ulp_distance(fl::expf(x), ::expf(x));
        if (d > worst) { worst = d; worst_x = x; }
    }
    FL_WARN("fl::expf worst error: " << worst << " ulp at x=" << worst_x);
    FL_CHECK_LE(worst, 2.0);
}

FL_TEST_CASE("fl::exp double matches libm to 2 ulp across the double range") {
    double worst = 0.0;
    double worst_x = 0.0;
    for (double x = -745.0; x < 709.7; x += 0.01) {
        const double d = ulp_distance(fl::exp(x), ::exp(x));
        if (d > worst) { worst = d; worst_x = x; }
    }
    FL_WARN("fl::exp worst error: " << worst << " ulp at x=" << worst_x);
    FL_CHECK_LE(worst, 2.0);
}

FL_TEST_CASE("fl::exp edges match libm") {
    FL_CHECK_EQ(fl::expf(0.0f), 1.0f);
    FL_CHECK_EQ(fl::exp(0.0), 1.0);
    // Overflow -> +inf, underflow -> 0, with no clamp constant in sight.
    FL_CHECK(fl::expf(89.0f) == INFINITY);
    FL_CHECK(fl::exp(710.0) == INFINITY);
    FL_CHECK_EQ(fl::expf(-104.0f), 0.0f);
    FL_CHECK_EQ(fl::exp(-746.0), 0.0);
    // Largest finite input stays finite.
    FL_CHECK(fl::expf(88.72f) < INFINITY);
    FL_CHECK(fl::exp(709.78) < INFINITY);
    // Subnormal results are produced, not flushed.
    FL_CHECK_GT(fl::expf(-100.0f), 0.0f);
    FL_CHECK_LE(ulp_distance(fl::expf(-100.0f), ::expf(-100.0f)), 2.0);
    // NaN in, NaN out.
    FL_CHECK(fl::expf(NAN) != fl::expf(NAN));
    // The values #4288 called out.
    FL_CHECK_LE(ulp_distance(fl::expf(-20.0f), ::expf(-20.0f)), 2.0);
    FL_CHECK_LE(ulp_distance(fl::expf(50.0f), ::expf(50.0f)), 2.0);
}

FL_TEST_CASE("fl::exp speed on the host vs libm") {
    // Informational. Inputs cover the constant-Q and Gaussian-window ranges.
    const int kN = 2000000;
    static float inputs[1024];
    for (int i = 0; i < 1024; ++i) inputs[i] = -10.0f + 20.0f * static_cast<float>(i) / 1023.0f;
    volatile float sink = 0.0f;
    const double ns_fl = ns_per_call(&fl::expf, inputs, kN, &sink);
    const double ns_libm = ns_per_call(&libm_expf, inputs, kN, &sink);
    const double ns_fl_d = ns_per_call(&fl_exp_double_as_float, inputs, kN, &sink);
    FL_WARN("exp ns/call on host: fl::expf=" << ns_fl << " libm_expf=" << ns_libm
            << " fl::exp(double)=" << ns_fl_d);
    FL_CHECK_GT(ns_fl, 0.0);
}


} // FL_TEST_FILE
