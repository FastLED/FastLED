#pragma once

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "crgb.h"
#include "fl/str.h"

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
}
