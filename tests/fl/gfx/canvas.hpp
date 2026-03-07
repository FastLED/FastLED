#pragma once

#include "test.h"
#include "fl/gfx.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("DrawCanvas with serpentine layout") {
    FL_SUBCASE("pixel (1,1) maps to correct index in 4x4") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructSerpentine(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        // Serpentine: row 0 = [0,1,2,3], row 1 = [7,6,5,4], row 2 = [8,9,10,11], row 3 = [15,14,13,12]
        // (1,1) should map to serpentine index 6 (second row, second from right)
        int expected_idx = xymap(1, 1);
        canvas.setPixel(1, 1, CRGB(100, 0, 0));
        FL_CHECK_EQ(buffer[expected_idx].r, 100);
    }

    FL_SUBCASE("pixel (0,0) is top-left in 4x4") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructSerpentine(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        canvas.setPixel(0, 0, CRGB(50, 0, 0));
        FL_CHECK_EQ(buffer[0].r, 50);
    }
}

FL_TEST_CASE("DrawCanvas with row-major layout") {
    FL_SUBCASE("pixel (1,1) maps to correct index in 4x4") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        // Row-major: row 1 starts at index 4, so (1,1) = 4 + 1 = 5
        int expected_idx = xymap(1, 1);
        canvas.setPixel(1, 1, CRGB(100, 0, 0));
        FL_CHECK_EQ(buffer[expected_idx].r, 100);
    }
}

FL_TEST_CASE("DrawCanvas bounds checking") {
    FL_SUBCASE("has() returns false for out-of-bounds") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        FL_CHECK(!canvas.has(-1, 0));
        FL_CHECK(!canvas.has(4, 0));
        FL_CHECK(!canvas.has(0, -1));
        FL_CHECK(!canvas.has(0, 4));
    }

    FL_SUBCASE("has() returns true for valid bounds") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        FL_CHECK(canvas.has(0, 0));
        FL_CHECK(canvas.has(3, 3));
        FL_CHECK(canvas.has(1, 1));
    }
}

FL_TEST_CASE("DrawCanvas pixel operations") {
    FL_SUBCASE("addPixel accumulates colors") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        canvas.addPixel(0, 0, CRGB(100, 0, 0));
        canvas.addPixel(0, 0, CRGB(100, 0, 0));
        FL_CHECK_EQ(buffer[0].r, 200);
    }

    FL_SUBCASE("setPixel overwrites color") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        canvas.setPixel(0, 0, CRGB(100, 0, 0));
        canvas.setPixel(0, 0, CRGB(50, 0, 0));
        FL_CHECK_EQ(buffer[0].r, 50);
    }

    FL_SUBCASE("out-of-bounds addPixel is safe") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};

        canvas.addPixel(-1, 0, CRGB(100, 0, 0));
        canvas.addPixel(4, 0, CRGB(100, 0, 0));
        // Should not crash, no pixels modified
        FL_CHECK_EQ(buffer[0].r, 0);
    }
}

FL_TEST_CASE("RasterTarget variant resolution") {
    FL_SUBCASE("RasterTarget from DrawCanvas") {
        CRGB buffer[16] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(4, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 16), xymap};
        fl::gfx::RasterTarget target = canvas;

        auto resolved = fl::gfx::resolveCanvas(target);
        resolved.setPixel(1, 1, CRGB(100, 0, 0));
        FL_CHECK_EQ(buffer[5].r, 100);  // row-major: (1,1) = 5
    }
}

FL_TEST_CASE("DrawCanvas dimensions") {
    FL_SUBCASE("width and height queries") {
        CRGB buffer[12] = {};
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(3, 4);
        fl::gfx::DrawCanvas canvas{fl::span<CRGB>(buffer, 12), xymap};

        FL_CHECK_EQ(canvas.width(), 3);
        FL_CHECK_EQ(canvas.height(), 4);
    }
}

} // FL_TEST_FILE
