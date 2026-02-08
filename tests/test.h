#pragma once

#include "doctest.h"

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
#include "fl/str.h"
#include "fl/stl/strstream.h"
#include "fl/tile2x2.h"
#include "fl/stl/vector.h"
#include "fl/xypath.h"

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
        CHECK(_result);                                                        \
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
        REQUIRE(_result);                                                      \
    } while (0)

// =============================================================================
// TRAMPOLINE MACROS - Abstraction layer for test framework
// =============================================================================
// These macros wrap the underlying doctest framework, allowing future
// framework changes without modifying all test files. This pattern is already
// established by CHECK_CLOSE and REQUIRE_CLOSE above.

// ----------------------------------------------------------------------------
// Basic Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK(expr)                     CHECK(expr)
#define FL_CHECK_FALSE(expr)               CHECK_FALSE(expr)
#define FL_CHECK_TRUE(expr)                CHECK_UNARY(expr)

// ----------------------------------------------------------------------------
// Comparison Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK_EQ(a, b)                  CHECK_EQ(a, b)
#define FL_CHECK_NE(a, b)                  CHECK_NE(a, b)
#define FL_CHECK_GT(a, b)                  CHECK_GT(a, b)
#define FL_CHECK_GE(a, b)                  CHECK_GE(a, b)
#define FL_CHECK_LT(a, b)                  CHECK_LT(a, b)
#define FL_CHECK_LE(a, b)                  CHECK_LE(a, b)

// ----------------------------------------------------------------------------
// Floating Point Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK_CLOSE(a, b, eps)          CHECK_CLOSE(a, b, eps)
#define FL_CHECK_DOUBLE_EQ(a, b)           CHECK_DOUBLE_EQ(a, b)
#define FL_CHECK_APPROX(a, b)              CHECK_APPROX(a, b)

// ----------------------------------------------------------------------------
// String Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK_STREQ(a, b)               CHECK_STREQ(a, b)

// ----------------------------------------------------------------------------
// Message Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK_MESSAGE(expr, msg)        CHECK_MESSAGE(expr, msg)
#define FL_CHECK_FALSE_MESSAGE(expr, msg)  CHECK_FALSE_MESSAGE(expr, msg)

// ----------------------------------------------------------------------------
// Trait Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK_TRAIT(...)                CHECK_TRAIT(__VA_ARGS__)

// ----------------------------------------------------------------------------
// Unary Assertions (CHECK family)
// ----------------------------------------------------------------------------
#define FL_CHECK_UNARY(expr)               CHECK_UNARY(expr)
#define FL_CHECK_UNARY_FALSE(expr)         CHECK_UNARY_FALSE(expr)

// ----------------------------------------------------------------------------
// Fatal Assertions (REQUIRE family - test stops on failure)
// ----------------------------------------------------------------------------
#define FL_REQUIRE(expr)                   REQUIRE(expr)
#define FL_REQUIRE_FALSE(expr)             REQUIRE_FALSE(expr)

// ----------------------------------------------------------------------------
// Comparison Assertions (REQUIRE family)
// ----------------------------------------------------------------------------
#define FL_REQUIRE_EQ(a, b)                REQUIRE_EQ(a, b)
#define FL_REQUIRE_NE(a, b)                REQUIRE_NE(a, b)
#define FL_REQUIRE_GT(a, b)                REQUIRE_GT(a, b)
#define FL_REQUIRE_GE(a, b)                REQUIRE_GE(a, b)
#define FL_REQUIRE_LT(a, b)                REQUIRE_LT(a, b)
#define FL_REQUIRE_LE(a, b)                REQUIRE_LE(a, b)

// ----------------------------------------------------------------------------
// Floating Point Assertions (REQUIRE family)
// ----------------------------------------------------------------------------
#define FL_REQUIRE_CLOSE(a, b, eps)        REQUIRE_CLOSE(a, b, eps)
#define FL_REQUIRE_APPROX(a, b)            REQUIRE_APPROX(a, b)

// ----------------------------------------------------------------------------
// Message Assertions (REQUIRE family)
// ----------------------------------------------------------------------------
#define FL_REQUIRE_MESSAGE(expr, msg)      REQUIRE_MESSAGE(expr, msg)

// ----------------------------------------------------------------------------
// Unary Assertions (REQUIRE family)
// ----------------------------------------------------------------------------
#define FL_REQUIRE_UNARY(expr)             REQUIRE_UNARY(expr)
#define FL_REQUIRE_UNARY_FALSE(expr)       REQUIRE_UNARY_FALSE(expr)

// ----------------------------------------------------------------------------
// Exception Handling (REQUIRE family)
// ----------------------------------------------------------------------------
#define FL_REQUIRE_THROWS_WITH_MESSAGE(...)  REQUIRE_THROWS_WITH_MESSAGE(__VA_ARGS__)

// =============================================================================
// END TRAMPOLINE MACROS
// =============================================================================

namespace doctest {
template <> struct StringMaker<CRGB> {
    static String convert(const CRGB &value) {
        fl::string out = value.toString();
        return out.c_str();
    }
};

template <> struct StringMaker<fl::string> {
    static String convert(const fl::string &value) { return value.c_str(); }
};

template <typename T> struct StringMaker<fl::optional<T>> {
    static String convert(const fl::optional<T> &value) {
        return fl::to_string(value).c_str();
    }
};

// Specialization for optional<T&&> which cannot be copied
template <typename T> struct StringMaker<fl::Optional<T&&>> {
    static String convert(const fl::Optional<T&&> &value) {
        if (!value.has_value()) {
            return "nullopt";
        }
        fl::string out = "optional(";
        out += *value;
        out += ")";
        return out.c_str();
    }
};

template <typename T> struct StringMaker<fl::vec2<T>> {
    static String convert(const fl::vec2<T> &value) {
        fl::string out;
        out += "vec2(";
        out += value.x;
        out += ", ";
        out += value.y;
        out += ")";
        return out.c_str();
    }
};

template <> struct StringMaker<fl::Tile2x2_u8> {
    static String convert(const fl::Tile2x2_u8 &value) {
        fl::sstream out;
        out << "Tile2x2_u8(" << value.origin() << ")";
        return out.c_str();
    }
};

template <typename T> struct StringMaker<fl::rect<T>> {
    static String convert(const fl::rect<T> &value) {
        fl::string out;
        out += "rect(";
        out += " (";
        out += value.mMin.x;
        out += ",";
        out += value.mMin.y;
        out += "), (";
        out += value.mMax.x;
        out += ",";
        out += value.mMax.y;
        out += "))";
        return out.c_str();
    }
};

template <typename Key, typename Hash, typename KeyEqual>
struct StringMaker<fl::unordered_set<Key, Hash, KeyEqual>> {
    static String convert(const fl::unordered_set<Key, Hash, KeyEqual> &value) {
        fl::string out;
        out.append(value);
        return out.c_str();
    }
};

template <typename T, typename Alloc> struct StringMaker<fl::vector<T, Alloc>> {
    static String convert(const fl::vector<T, Alloc> &value) {
        fl::string out;
        out.append(value);
        return out.c_str();
    }
};
} // namespace doctest
