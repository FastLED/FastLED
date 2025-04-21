#pragma once

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "crgb.h"
#include "fl/str.h"
#include "fl/lut.h"
#include "fl/xypath.h"
#include "fl/subpixel.h"

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

    template<typename T> struct StringMaker<point_xy<T>> {
        static String convert(const point_xy<T>& value) {
            fl::Str out;
            out += "point_xy(";
            out += value.x;
            out += ", ";
            out += value.y;
            out += ")";
            return out.c_str();
        }
    };

    template<> struct StringMaker<SubPixel2x2> {
        static String convert(const SubPixel2x2& value) {
            fl::Str out;
            out += "SubPixel2x2(";
            out += value.origin().x;
            out += ", ";
            out += value.origin().y;
            out += ")";
            return out.c_str();
        }
    };
}
