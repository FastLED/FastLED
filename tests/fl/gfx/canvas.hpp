#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("Canvas basic operations") {
    FL_SUBCASE("at() reads and writes pixels") {
        CRGB buffer[16] = {};
        fl::CanvasRGB canvas(buffer, 4, 4);

        canvas.at(1, 2) = CRGB(100, 0, 0);
        FL_CHECK_EQ(buffer[2 * 4 + 1].r, 100);
    }

    FL_SUBCASE("has() returns correct bounds") {
        CRGB buffer[16] = {};
        fl::CanvasRGB canvas(buffer, 4, 4);

        FL_CHECK(canvas.has(0, 0));
        FL_CHECK(canvas.has(3, 3));
        FL_CHECK(!canvas.has(-1, 0));
        FL_CHECK(!canvas.has(4, 0));
        FL_CHECK(!canvas.has(0, -1));
        FL_CHECK(!canvas.has(0, 4));
    }

    FL_SUBCASE("size() returns width * height") {
        CRGB buffer[12] = {};
        fl::CanvasRGB canvas(buffer, 3, 4);

        FL_CHECK_EQ(canvas.size(), 12);
    }
}

} // FL_TEST_FILE
