#pragma once

#include "doctest.h"

#include <stdint.h>
#include <vector>
#include <set>
#include <string>
#include <iostream>

#include "crgb.h"
#include "fl/str.h"
#include "fl/lut.h"
#include "fl/xypath.h"
#include "fl/tile2x2.h"
#include "fl/strstream.h"
#include "fl/hash_set.h"
#include "fl/vector.h"


using namespace fl;

namespace doctest {
    template<> struct StringMaker<CRGB> {
        static String convert(const CRGB& value) {
            fl::Str out = value.toString();
            return out.c_str();
        }
    };

    template<> struct StringMaker<Str> {
        static String convert(const Str& value) {
            return value.c_str();
        }
    };

    template<typename T> struct StringMaker<vec2<T>> {
        static String convert(const vec2<T>& value) {
            fl::Str out;
            out += "vec2(";
            out += value.x;
            out += ", ";
            out += value.y;
            out += ")";
            return out.c_str();
        }
    };

    template<> struct StringMaker<Tile2x2_u8> {
        static String convert(const Tile2x2_u8& value) {
            fl::StrStream out;
            out << "Tile2x2_u8(" << value.origin() << ")";
            return out.c_str();
        }
    };

    template<typename T> struct StringMaker<rect<T>> {
        static String convert(const rect<T>& value) {
            fl::Str out;
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


    template<typename Key, typename Hash, typename KeyEqual> struct StringMaker<fl::hash_set<Key, Hash, KeyEqual>> {
        static String convert(const fl::hash_set<Key, Hash, KeyEqual>& value) {
            fl::Str out;
            out.append(value);
            return out.c_str();
        }
    };

    template<typename T>
    struct StringMaker<fl::vector<T>> {
        static String convert(const fl::vector<T>& value) {
            fl::Str out;
            out.append(value);
            return out.c_str();
        }
    };
}
