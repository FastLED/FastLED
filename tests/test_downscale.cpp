
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/downscale.h"
#include "fl/dbg.h"
#include "test.h"

using namespace fl;

TEST_CASE("downscale 2x2 to 1x1") {

    CRGB red = CRGB(255, 0, 0);
    CRGB black = CRGB(0, 0, 0);

    // We are going to simulate a 4x4 image with a 2x2 image. The source
    // image is square-cartesian while the dst image is square-serpentine.
    CRGB src[4] = {black, red, black, red};

    SUBCASE("downscaleHalf from 2x2 to 1x1") {
        CRGB dst[1];
        downscaleHalf(src, 2, 2, dst);
        INFO("Src: " << src);
        INFO("Dst: " << dst);
        CHECK(dst[0].r == 128);
        CHECK(dst[0].g == 0);
        CHECK(dst[0].b == 0);
    }

    SUBCASE("downscale from 2x2 to 1x1") {
        CRGB dst[1];
        XYMap srcMap = XYMap::constructRectangularGrid(2, 2);
        XYMap dstMap = XYMap::constructRectangularGrid(1, 1);

        downscale(src, srcMap, dst, dstMap);
        INFO("Src: " << src);
        INFO("Dst: " << dst);
        CHECK(dst[0].r == 128);
        CHECK(dst[0].g == 0);
        CHECK(dst[0].b == 0);
    }


    SUBCASE("4x4 rectangle to 2x2 serpentine") {
        // We are going to simulate a 4x4 image with a 2x2 image. The source
        // image is square-cartesian while the dst image is square-serpentine.

        CRGB src[16] = {// Upper left red, lower right red, upper right black,
                        // lower left black
                        red,   red,   black, black, red,   red,   black, black,
                        black, black, red,   red,   black, black, red,   red};

        CRGB dst[4];

        XYMap srcMap = XYMap::constructRectangularGrid(4, 4);
        XYMap dstMap = XYMap::constructSerpentine(2, 2);

        downscale(src, srcMap, dst, dstMap);
        INFO("Src: " << src);
        INFO("Dst: " << dst);

        CRGB lowerLeft = dst[dstMap.mapToIndex(0, 0)];
        CRGB lowerRight = dst[dstMap.mapToIndex(1, 0)];
        CRGB upperLeft = dst[dstMap.mapToIndex(0, 1)];
        CRGB upperRight = dst[dstMap.mapToIndex(1, 1)];

        REQUIRE(lowerLeft == red);
        REQUIRE(lowerRight == black);
        REQUIRE(upperLeft == black);
        REQUIRE(upperRight == red);
    }

}

TEST_CASE("downscale 3x3 to 2x2") {
    CRGB red = CRGB(255, 0, 0);
    CRGB black = CRGB(0, 0, 0);

    // Create a 3x3 checkerboard pattern:
    CRGB src[9];
    src[0] = red;
    src[1] = black;
    src[2] = red;
    src[3] = black;
    src[4] = red;
    src[5] = black;
    src[6] = red;
    src[7] = black;
    src[8] = red;

    CRGB dst[4];  // 2x2 result

    XYMap srcMap = XYMap::constructRectangularGrid(3, 3);
    XYMap dstMap = XYMap::constructRectangularGrid(2, 2);

    downscale(src, srcMap, dst, dstMap);

    for (int i = 0; i < 4; ++i) {
        INFO("Dst[" << i << "]: " << dst[i]);
        CHECK(dst[i] == CRGB(142, 0, 0));  // Averaged color
    }
}


TEST_CASE("downscale 11x11 to 2x2") {
    CRGB red = CRGB(255, 0, 0);
    CRGB black = CRGB(0, 0, 0);

    // Create a 3x3 checkerboard pattern:
    CRGB src[11*11];
    for (int i = 0; i < 11*11; ++i) {
        src[i] = (i % 2 == 0) ? red : black;
    }

    CRGB dst[4];  // 2x2 result

    XYMap srcMap = XYMap::constructRectangularGrid(11, 11);
    XYMap dstMap = XYMap::constructRectangularGrid(2, 2);

    downscale(src, srcMap, dst, dstMap);

    for (int i = 0; i < 4; ++i) {
        INFO("Dst[" << i << "]: " << dst[i]);
        CHECK(dst[i] == CRGB(129, 0, 0));  // Averaged color
    }
}