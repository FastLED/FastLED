#include "fl/stl/math.h"
#include "doctest.h"
#include "fl/math_macros.h"

using namespace fl;

TEST_CASE("fl::floor") {
    SUBCASE("float version") {
        FL_CHECK_EQ(fl::floorf(3.7f), 3.0f);
        FL_CHECK_EQ(fl::floorf(3.0f), 3.0f);
        FL_CHECK_EQ(fl::floorf(-3.7f), -4.0f);
        FL_CHECK_EQ(fl::floorf(-3.0f), -3.0f);
        FL_CHECK_EQ(fl::floorf(0.0f), 0.0f);
    }

    SUBCASE("double version") {
        FL_CHECK_EQ(fl::floor(3.7), 3.0);
        FL_CHECK_EQ(fl::floor(3.0), 3.0);
        FL_CHECK_EQ(fl::floor(-3.7), -4.0);
        FL_CHECK_EQ(fl::floor(-3.0), -3.0);
        FL_CHECK_EQ(fl::floor(0.0), 0.0);
    }
}

TEST_CASE("fl::ceil") {
    SUBCASE("float version") {
        FL_CHECK_EQ(fl::ceilf(3.2f), 4.0f);
        FL_CHECK_EQ(fl::ceilf(3.0f), 3.0f);
        FL_CHECK_EQ(fl::ceilf(-3.2f), -3.0f);
        FL_CHECK_EQ(fl::ceilf(-3.0f), -3.0f);
        FL_CHECK_EQ(fl::ceilf(0.0f), 0.0f);
    }

    SUBCASE("double version") {
        FL_CHECK_EQ(fl::ceil(3.2), 4.0);
        FL_CHECK_EQ(fl::ceil(3.0), 3.0);
        FL_CHECK_EQ(fl::ceil(-3.2), -3.0);
        FL_CHECK_EQ(fl::ceil(-3.0), -3.0);
        FL_CHECK_EQ(fl::ceil(0.0), 0.0);
    }

    SUBCASE("constexpr version") {
        constexpr int result1 = fl::ceil_constexpr(3.2f);
        constexpr int result2 = fl::ceil_constexpr(3.0f);
        constexpr int result3 = fl::ceil_constexpr(-3.2f);
        FL_CHECK_EQ(result1, 4);
        FL_CHECK_EQ(result2, 3);
        FL_CHECK_EQ(result3, -3);
    }
}

TEST_CASE("fl::sqrt") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::sqrtf(4.0f)).epsilon(1e-5) == 2.0f);
        FL_CHECK(doctest::Approx(fl::sqrtf(9.0f)).epsilon(1e-5) == 3.0f);
        FL_CHECK(doctest::Approx(fl::sqrtf(2.0f)).epsilon(1e-5) == 1.414213562f);
        FL_CHECK_EQ(fl::sqrtf(0.0f), 0.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::sqrt(4.0)).epsilon(1e-10) == 2.0);
        FL_CHECK(doctest::Approx(fl::sqrt(9.0)).epsilon(1e-10) == 3.0);
        FL_CHECK(doctest::Approx(fl::sqrt(2.0)).epsilon(1e-10) == 1.41421356237);
        FL_CHECK_EQ(fl::sqrt(0.0), 0.0);
    }
}

TEST_CASE("fl::exp") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::expf(0.0f)).epsilon(0.001f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::expf(1.0f)).epsilon(0.001f) == 2.71828f);
        FL_CHECK(doctest::Approx(fl::expf(2.0f)).epsilon(0.001f) == 7.38906f);
        FL_CHECK(doctest::Approx(fl::expf(-1.0f)).epsilon(0.001f) == 0.36788f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::exp(0.0)).epsilon(0.001) == 1.0);
        FL_CHECK(doctest::Approx(fl::exp(1.0)).epsilon(0.001) == 2.71828182845);
        FL_CHECK(doctest::Approx(fl::exp(2.0)).epsilon(0.001) == 7.38905609893);
        FL_CHECK(doctest::Approx(fl::exp(-1.0)).epsilon(0.001) == 0.36787944117);
    }
}

TEST_CASE("fl::sin") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::sinf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::sinf(FL_PI / 2.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::sinf(FL_PI)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::sinf(-FL_PI / 2.0f)).epsilon(1e-6f) == -1.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::sin(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::sin(FL_PI / 2.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::sin(FL_PI)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::sin(-FL_PI / 2.0)).epsilon(1e-10) == -1.0);
    }
}

TEST_CASE("fl::cos") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::cosf(0.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::cosf(FL_PI / 2.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::cosf(FL_PI)).epsilon(1e-6f) == -1.0f);
        FL_CHECK(doctest::Approx(fl::cosf(-FL_PI)).epsilon(1e-6f) == -1.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::cos(0.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::cos(FL_PI / 2.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::cos(FL_PI)).epsilon(1e-10) == -1.0);
        FL_CHECK(doctest::Approx(fl::cos(-FL_PI)).epsilon(1e-10) == -1.0);
    }
}

TEST_CASE("fl::tan") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::tanf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::tanf(FL_PI / 4.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::tanf(-FL_PI / 4.0f)).epsilon(1e-6f) == -1.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::tan(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::tan(FL_PI / 4.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::tan(-FL_PI / 4.0)).epsilon(1e-10) == -1.0);
    }
}

TEST_CASE("fl::log") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::logf(1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::logf(2.71828f)).epsilon(0.001f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::logf(7.38906f)).epsilon(0.001f) == 2.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::log(1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::log(2.71828182845)).epsilon(0.001) == 1.0);
        FL_CHECK(doctest::Approx(fl::log(7.38905609893)).epsilon(0.001) == 2.0);
    }
}

TEST_CASE("fl::log10") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::log10f(1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::log10f(10.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::log10f(100.0f)).epsilon(1e-6f) == 2.0f);
        FL_CHECK(doctest::Approx(fl::log10f(1000.0f)).epsilon(1e-6f) == 3.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::log10(1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::log10(10.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::log10(100.0)).epsilon(1e-10) == 2.0);
        FL_CHECK(doctest::Approx(fl::log10(1000.0)).epsilon(1e-10) == 3.0);
    }
}

TEST_CASE("fl::pow") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 0.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 1.0f)).epsilon(1e-6f) == 2.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 2.0f)).epsilon(1e-6f) == 4.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, 3.0f)).epsilon(1e-6f) == 8.0f);
        FL_CHECK(doctest::Approx(fl::powf(3.0f, 2.0f)).epsilon(1e-6f) == 9.0f);
        FL_CHECK(doctest::Approx(fl::powf(2.0f, -1.0f)).epsilon(1e-6f) == 0.5f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::pow(2.0, 0.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, 1.0)).epsilon(1e-10) == 2.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, 2.0)).epsilon(1e-10) == 4.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, 3.0)).epsilon(1e-10) == 8.0);
        FL_CHECK(doctest::Approx(fl::pow(3.0, 2.0)).epsilon(1e-10) == 9.0);
        FL_CHECK(doctest::Approx(fl::pow(2.0, -1.0)).epsilon(1e-10) == 0.5);
    }
}

TEST_CASE("fl::fabs") {
    SUBCASE("float version") {
        FL_CHECK_EQ(fl::fabsf(3.5f), 3.5f);
        FL_CHECK_EQ(fl::fabsf(-3.5f), 3.5f);
        FL_CHECK_EQ(fl::fabsf(0.0f), 0.0f);
    }

    SUBCASE("double version") {
        FL_CHECK_EQ(fl::fabs(3.5), 3.5);
        FL_CHECK_EQ(fl::fabs(-3.5), 3.5);
        FL_CHECK_EQ(fl::fabs(0.0), 0.0);
    }
}

TEST_CASE("fl::lround") {
    SUBCASE("float version") {
        FL_CHECK_EQ(fl::lroundf(3.5f), 4);
        FL_CHECK_EQ(fl::lroundf(3.4f), 3);
        FL_CHECK_EQ(fl::lroundf(-3.5f), -4);
        FL_CHECK_EQ(fl::lroundf(-3.4f), -3);
        FL_CHECK_EQ(fl::lroundf(0.0f), 0);
    }

    SUBCASE("double version") {
        FL_CHECK_EQ(fl::lround(3.5), 4);
        FL_CHECK_EQ(fl::lround(3.4), 3);
        FL_CHECK_EQ(fl::lround(-3.5), -4);
        FL_CHECK_EQ(fl::lround(-3.4), -3);
        FL_CHECK_EQ(fl::lround(0.0), 0);
    }
}

TEST_CASE("fl::fmod") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::fmodf(5.0f, 2.0f)).epsilon(1e-6f) == 1.0f);
        FL_CHECK(doctest::Approx(fl::fmodf(6.0f, 3.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::fmodf(7.5f, 2.5f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::fmodf(-5.0f, 2.0f)).epsilon(1e-6f) == -1.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::fmod(5.0, 2.0)).epsilon(1e-10) == 1.0);
        FL_CHECK(doctest::Approx(fl::fmod(6.0, 3.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::fmod(7.5, 2.5)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::fmod(-5.0, 2.0)).epsilon(1e-10) == -1.0);
    }
}

TEST_CASE("fl::atan2") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::atan2f(0.0f, 1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::atan2f(1.0f, 0.0f)).epsilon(1e-6f) == FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::atan2f(0.0f, -1.0f)).epsilon(1e-6f) == FL_PI);
        FL_CHECK(doctest::Approx(fl::atan2f(-1.0f, 0.0f)).epsilon(1e-6f) == -FL_PI / 2.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::atan2(0.0, 1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::atan2(1.0, 0.0)).epsilon(1e-10) == FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::atan2(0.0, -1.0)).epsilon(1e-10) == FL_PI);
        FL_CHECK(doctest::Approx(fl::atan2(-1.0, 0.0)).epsilon(1e-10) == -FL_PI / 2.0);
    }
}

TEST_CASE("fl::hypot") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::hypotf(3.0f, 4.0f)).epsilon(1e-6f) == 5.0f);
        FL_CHECK(doctest::Approx(fl::hypotf(0.0f, 0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::hypotf(1.0f, 1.0f)).epsilon(0.001f) == 1.41421f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::hypot(3.0, 4.0)).epsilon(1e-10) == 5.0);
        FL_CHECK(doctest::Approx(fl::hypot(0.0, 0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::hypot(1.0, 1.0)).epsilon(0.001) == 1.41421356237);
    }
}

TEST_CASE("fl::atan") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::atanf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::atanf(1.0f)).epsilon(1e-6f) == FL_PI / 4.0f);
        FL_CHECK(doctest::Approx(fl::atanf(-1.0f)).epsilon(1e-6f) == -FL_PI / 4.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::atan(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::atan(1.0)).epsilon(1e-10) == FL_PI / 4.0);
        FL_CHECK(doctest::Approx(fl::atan(-1.0)).epsilon(1e-10) == -FL_PI / 4.0);
    }
}

TEST_CASE("fl::asin") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::asinf(0.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::asinf(1.0f)).epsilon(1e-6f) == FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::asinf(-1.0f)).epsilon(1e-6f) == -FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::asinf(0.5f)).epsilon(1e-6f) == FL_PI / 6.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::asin(0.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::asin(1.0)).epsilon(1e-10) == FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::asin(-1.0)).epsilon(1e-10) == -FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::asin(0.5)).epsilon(1e-10) == FL_PI / 6.0);
    }
}

TEST_CASE("fl::acos") {
    SUBCASE("float version") {
        FL_CHECK(doctest::Approx(fl::acosf(1.0f)).epsilon(1e-6f) == 0.0f);
        FL_CHECK(doctest::Approx(fl::acosf(0.0f)).epsilon(1e-6f) == FL_PI / 2.0f);
        FL_CHECK(doctest::Approx(fl::acosf(-1.0f)).epsilon(1e-6f) == FL_PI);
        FL_CHECK(doctest::Approx(fl::acosf(0.5f)).epsilon(1e-6f) == FL_PI / 3.0f);
    }

    SUBCASE("double version") {
        FL_CHECK(doctest::Approx(fl::acos(1.0)).epsilon(1e-10) == 0.0);
        FL_CHECK(doctest::Approx(fl::acos(0.0)).epsilon(1e-10) == FL_PI / 2.0);
        FL_CHECK(doctest::Approx(fl::acos(-1.0)).epsilon(1e-10) == FL_PI);
        FL_CHECK(doctest::Approx(fl::acos(0.5)).epsilon(1e-10) == FL_PI / 3.0);
    }
}

TEST_CASE("math macros") {
    SUBCASE("FL_MIN and FL_MAX") {
        FL_CHECK_EQ(FL_MIN(5, 10), 5);
        FL_CHECK_EQ(FL_MAX(5, 10), 10);
        FL_CHECK_EQ(FL_MIN(-5, -10), -10);
        FL_CHECK_EQ(FL_MAX(-5, -10), -5);
    }

    SUBCASE("FL_ABS") {
        FL_CHECK_EQ(FL_ABS(5), 5);
        FL_CHECK_EQ(FL_ABS(-5), 5);
        FL_CHECK_EQ(FL_ABS(0), 0);
    }

    SUBCASE("FL_ALMOST_EQUAL") {
        FL_CHECK(FL_ALMOST_EQUAL(1.0f, 1.00001f, 0.001f));
        FL_CHECK_FALSE(FL_ALMOST_EQUAL(1.0f, 1.01f, 0.001f));
    }

    SUBCASE("FL_ALMOST_EQUAL_FLOAT") {
        FL_CHECK(FL_ALMOST_EQUAL_FLOAT(1.0f, 1.0f + FL_EPSILON_F / 2.0f));
    }

    SUBCASE("FL_ALMOST_EQUAL_DOUBLE") {
        FL_CHECK(FL_ALMOST_EQUAL_DOUBLE(1.0, 1.0 + FL_EPSILON_D / 2.0));
    }

    SUBCASE("FL_PI") {
        FL_CHECK(doctest::Approx(static_cast<double>(FL_PI)).epsilon(1e-10) == 3.141592653589793);
    }
}
