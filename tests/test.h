#pragma once

#include "doctest.h"

#include "fl/stdint.h"
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "crgb.h"
#include "fl/hash_set.h"
#include "fl/lut.h"
#include "fl/optional.h"
#include "fl/str.h"
#include "fl/strstream.h"
#include "fl/tile2x2.h"
#include "fl/vector.h"
#include "fl/xypath.h"

// Define an improved CHECK_CLOSE macro that provides better error messages
#define CHECK_CLOSE(a, b, epsilon)                                             \
    do {                                                                       \
        float _a = (a);                                                        \
        float _b = (b);                                                        \
        float _diff = fabsf(_a - _b);                                          \
        bool _result = _diff <= (epsilon);                                     \
        if (!_result) {                                                        \
            printf("CHECK_CLOSE failed: |%f - %f| = %f > %f\n", (float)_a,     \
                   (float)_b, _diff, (float)(epsilon));                        \
        }                                                                      \
        CHECK(_result);                                                        \
    } while (0)

#define REQUIRE_CLOSE(a, b, epsilon)                                           \
    do {                                                                       \
        float _a = (a);                                                        \
        float _b = (b);                                                        \
        float _diff = fabsf(_a - _b);                                          \
        bool _result = _diff <= (epsilon);                                     \
        if (!_result) {                                                        \
            printf("REQUIRE_CLOSE failed: |%f - %f| = %f > %f\n", (float)_a,   \
                   (float)_b, _diff, (float)(epsilon));                        \
        }                                                                      \
        REQUIRE(_result);                                                      \
    } while (0)

using namespace fl;

namespace doctest {
template <> struct StringMaker<CRGB> {
    static String convert(const CRGB &value) {
        fl::string out = value.toString();
        return out.c_str();
    }
};

template <> struct StringMaker<Str> {
    static String convert(const Str &value) { return value.c_str(); }
};

template <typename T> struct StringMaker<fl::optional<T>> {
    static String convert(const fl::optional<T> &value) {
        return fl::to_string(value).c_str();
    }
};

template <typename T> struct StringMaker<vec2<T>> {
    static String convert(const vec2<T> &value) {
        fl::string out;
        out += "vec2(";
        out += value.x;
        out += ", ";
        out += value.y;
        out += ")";
        return out.c_str();
    }
};

template <> struct StringMaker<Tile2x2_u8> {
    static String convert(const Tile2x2_u8 &value) {
        fl::StrStream out;
        out << "Tile2x2_u8(" << value.origin() << ")";
        return out.c_str();
    }
};

template <typename T> struct StringMaker<rect<T>> {
    static String convert(const rect<T> &value) {
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
struct StringMaker<fl::hash_set<Key, Hash, KeyEqual>> {
    static String convert(const fl::hash_set<Key, Hash, KeyEqual> &value) {
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
