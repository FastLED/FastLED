#pragma once

#include "shared/test_registry.h"
#include "shared/fl_unittest.h"
#include "fl/stl/stdint.h"
#include "fl/stl/iostream.h"
#include "fl/stl/cstring.h"
#include "fl/stl/set.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdio.h"
#include "crgb.h"
#include "fl/stl/unordered_set.h"
#include "fl/lut.h"
#include "fl/stl/optional.h"
#include "fl/stl/strstream.h"
#include "fl/tile2x2.h"
#include "fl/xypath.h"

// ============================================================================
// Compatibility layer for doctest types and macros
// ============================================================================
namespace doctest {
    // Helper function to construct TestApprox with type deduction
    template<typename T>
    inline fl::test::TestApprox<T> Approx(T value) {
        return fl::test::TestApprox<T>(value);
    }

    // Type alias for when users explicitly specify the template parameter
    template<typename T>
    using Approx_t = fl::test::TestApprox<T>;
}

// Doctest-style assertion macros (compatibility aliases)
#define CHECK(expr) FL_CHECK(expr)
#define CHECK_FALSE(expr) FL_CHECK_FALSE(expr)
#define CHECK_TRUE(expr) FL_CHECK_TRUE(expr)
#define CHECK_EQ(a, b) FL_CHECK_EQ(a, b)
#define CHECK_NE(a, b) FL_CHECK_NE(a, b)
#define CHECK_LT(a, b) FL_CHECK_LT(a, b)
#define CHECK_LE(a, b) FL_CHECK_LE(a, b)
#define CHECK_GT(a, b) FL_CHECK_GT(a, b)
#define CHECK_GE(a, b) FL_CHECK_GE(a, b)
#define CHECK_THROWS(expr) FL_CHECK_THROWS(expr)
#define CHECK_THROWS_AS(expr, E) FL_CHECK_THROWS_AS(expr, E)
#define CHECK_NOTHROW(expr) FL_CHECK_NOTHROW(expr)
#define CHECK_MESSAGE(cond, msg) FL_CHECK_MESSAGE(cond, msg)

#define REQUIRE(expr) FL_REQUIRE(expr)
#define REQUIRE_FALSE(expr) FL_REQUIRE_FALSE(expr)
#define REQUIRE_TRUE(expr) FL_REQUIRE_TRUE(expr)
#define REQUIRE_EQ(a, b) FL_REQUIRE_EQ(a, b)
#define REQUIRE_NE(a, b) FL_REQUIRE_NE(a, b)
#define REQUIRE_LT(a, b) FL_REQUIRE_LT(a, b)
#define REQUIRE_LE(a, b) FL_REQUIRE_LE(a, b)
#define REQUIRE_GT(a, b) FL_REQUIRE_GT(a, b)
#define REQUIRE_GE(a, b) FL_REQUIRE_GE(a, b)
#define REQUIRE_THROWS(expr) FL_REQUIRE_THROWS(expr)
#define REQUIRE_THROWS_AS(expr, E) FL_REQUIRE_THROWS_AS(expr, E)
#define REQUIRE_NOTHROW(expr) FL_REQUIRE_NOTHROW(expr)
#define REQUIRE_MESSAGE(cond, msg) FL_REQUIRE_MESSAGE(cond, msg)

#define TEST_CASE(name, ...) FL_TEST_CASE(name, ##__VA_ARGS__)
#define SUBCASE(name) FL_SUBCASE(name)
#define MESSAGE(...) FL_MESSAGE(__VA_ARGS__)
#define FAIL(msg) FL_FAIL(msg)
#define FAIL_CHECK(msg) FL_FAIL_CHECK(msg)

#define FL_FILEPATH __FILE__
#define FL_TEST_FILE(path) namespace test

// FL_TEST_CASE macro is now fully defined in fl_unittest.h with:
// - Full decorator support (skip, tags)
// - Automatic registration to TestRegistry singleton
// - Automatic registration to fl::test framework
// - Native skip() decorator support (no #if 0 workarounds needed)

// Define an improved CHECK_CLOSE macro that provides better error messages
#define CHECK_CLOSE(a, b, epsilon)                                             \
    do {                                                                       \
        float _a = (a);                                                        \
        float _b = (b);                                                        \
        float _diff = fl::fabsf(_a - _b);                                      \
        bool _result = _diff <= (epsilon);                                     \
        if (!_result) {                                                        \
            fl::printf("CHECK_CLOSE failed: |%f - %f| = %f > %f\n", (float)_a, \
                       (float)_b, _diff, (float)(epsilon));                    \
        }                                                                      \
        FL_CHECK(_result);                                                     \
    } while (0)

#define REQUIRE_CLOSE(a, b, epsilon)                                           \
    do {                                                                       \
        float _a = (a);                                                        \
        float _b = (b);                                                        \
        float _diff = fl::fabsf(_a - _b);                                      \
        bool _result = _diff <= (epsilon);                                     \
        if (!_result) {                                                        \
            fl::printf("REQUIRE_CLOSE failed: |%f - %f| = %f > %f\n", (float)_a, \
                       (float)_b, _diff, (float)(epsilon));                      \
        }                                                                      \
        FL_REQUIRE(_result);                                                   \
    } while (0)

// FL_CHECK_CLOSE and FL_REQUIRE_CLOSE aliases (same as CHECK_CLOSE/REQUIRE_CLOSE)
#define FL_CHECK_CLOSE(a, b, epsilon) CHECK_CLOSE(a, b, epsilon)
#define FL_REQUIRE_CLOSE(a, b, epsilon) REQUIRE_CLOSE(a, b, epsilon)

// =============================================================================
// NOTE: All FL_* macros are now defined in fl_unittest.h
// =============================================================================
// The fl_unittest framework provides all assertion macros, subcase support,
// and decorator support. No additional trampoline macros are needed here.
//
// Available macros from fl_unittest.h:
// - FL_TEST_CASE, FL_SUBCASE, FL_TEST_CASE_FIXTURE, FL_TEST_SUITE
// - FL_CHECK*, FL_REQUIRE*, FL_DWARN*, FL_WARN*
// - FL_CHECK_THROWS, FL_REQUIRE_THROWS, FL_CHECK_NOTHROW, etc.
// - FL_FAIL, FL_FAIL_CHECK, FL_MESSAGE, FL_DINFO, FL_CAPTURE
// - FL_SCENARIO, FL_GIVEN, FL_WHEN, FL_THEN, FL_AND_WHEN, FL_AND_THEN
//
// String formatting uses fl::test::Stringify<T> specializations
// (defined in fl_unittest.h, not doctest::StringMaker)
