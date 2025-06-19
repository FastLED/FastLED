// Compile with: g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/cielab.h"

using namespace fl;

TEST_CASE("CIELAB16 basic operations") {

    SUBCASE("Conversion from CRGB to CIELAB16") {
        CRGB c(255, 255, 255);
        CIELAB16 lab(c);
        CRGB c2 = lab.ToRGB();
        CHECK(c2.r == c.r);
        CHECK(c2.g == c.g);
        CHECK(c2.b == c.b);
    }
}


TEST_CASE("CIELAB16 video operations") {

}