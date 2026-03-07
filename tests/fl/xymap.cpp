/// @file tests/fl/xymap.cpp
/// @brief Test suite for XYMap addressing transformations

#include "fl/xymap.h"
#include "fl/screenmap.h"
#include "fl/fltest.h"

using namespace fl;

// ============ XYMap Addressing Tests ============
// Tests for XYMap layout transformations (serpentine, rectangular, etc)

FL_TEST_CASE("ScreenMap stores source XYMap from toScreenMap conversion") {
    fl::XYMap xymap = fl::XYMap::constructSerpentine(4, 4);
    fl::ScreenMap screenmap = xymap.toScreenMap();

    FL_CHECK(screenmap.hasSourceXYMap());
    FL_CHECK_NE(screenmap.getSourceXYMapPtr().get(), nullptr);

    const fl::XYMap* source = screenmap.getSourceXYMapPtr().get();
    FL_CHECK_EQ(source->getWidth(), 4);
    FL_CHECK_EQ(source->getHeight(), 4);
}

FL_TEST_CASE("ScreenMap copy includes source XYMap") {
    fl::XYMap xymap = fl::XYMap::constructSerpentine(8, 8);
    fl::ScreenMap original = xymap.toScreenMap();

    fl::ScreenMap copy1 = original;
    FL_CHECK(copy1.hasSourceXYMap());

    fl::ScreenMap copy2;
    copy2 = original;
    FL_CHECK(copy2.hasSourceXYMap());

    fl::ScreenMap orig3 = fl::XYMap::constructSerpentine(16, 16).toScreenMap();
    fl::ScreenMap moved = fl::move(orig3);
    FL_CHECK(moved.hasSourceXYMap());
}

FL_TEST_CASE("XYMap serpentine layout produces correct addressing") {
    fl::XYMap serpentine = fl::XYMap::constructSerpentine(2, 4);

    FL_CHECK_EQ(serpentine.mapToIndex(0, 0), 0);
    FL_CHECK_EQ(serpentine.mapToIndex(1, 0), 1);
    FL_CHECK_EQ(serpentine.mapToIndex(0, 1), 3);
    FL_CHECK_EQ(serpentine.mapToIndex(1, 1), 2);
    FL_CHECK_EQ(serpentine.mapToIndex(0, 2), 4);
    FL_CHECK_EQ(serpentine.mapToIndex(1, 2), 5);
    FL_CHECK_EQ(serpentine.mapToIndex(0, 3), 7);
    FL_CHECK_EQ(serpentine.mapToIndex(1, 3), 6);
}

FL_TEST_CASE("XYMap mapPixels applies serpentine transformation correctly") {
    CRGB input[6] = {
        CRGB::Red,      CRGB::Green,
        CRGB::Blue,     CRGB::Yellow,
        CRGB::Magenta,  CRGB::Cyan
    };

    CRGB output[6] = {CRGB::Black};

    fl::XYMap serpentine = fl::XYMap::constructSerpentine(2, 3);
    serpentine.mapPixels(input, output);

    FL_CHECK(output[0] == CRGB::Red);
    FL_CHECK(output[1] == CRGB::Green);
    FL_CHECK(output[2] == CRGB::Yellow);
    FL_CHECK(output[3] == CRGB::Blue);
    FL_CHECK(output[4] == CRGB::Magenta);
    FL_CHECK(output[5] == CRGB::Cyan);
}

FL_TEST_CASE("XYMap rectangular grid is identity transformation") {
    CRGB input[4] = {
        CRGB::Red,    CRGB::Green,
        CRGB::Blue,   CRGB::Yellow
    };

    CRGB output[4] = {CRGB::Black};

    fl::XYMap rectangular = fl::XYMap::constructRectangularGrid(2, 2);
    rectangular.mapPixels(input, output);

    for (int i = 0; i < 4; i++) {
        FL_CHECK_EQ(output[i].r, input[i].r);
        FL_CHECK_EQ(output[i].g, input[i].g);
        FL_CHECK_EQ(output[i].b, input[i].b);
    }
}

FL_TEST_CASE("XYMap transformation with different sizes") {
    fl::XYMap serpentine = fl::XYMap::constructSerpentine(4, 2);

    FL_CHECK_EQ(serpentine.getWidth(), 4);
    FL_CHECK_EQ(serpentine.getHeight(), 2);
    FL_CHECK(serpentine.isSerpentine());
}

FL_TEST_CASE("Edge case: 1x1 buffer with addressing") {
    CRGB input[1] = {CRGB::Red};
    CRGB output[1] = {CRGB::Black};

    fl::XYMap addressing = fl::XYMap::constructSerpentine(1, 1);
    addressing.mapPixels(input, output);

    FL_CHECK(output[0] == CRGB::Red);
}

FL_TEST_CASE("Edge case: Wide strip (1 row, many LEDs)") {
    const int NUM_LEDS = 32;
    CRGB input[NUM_LEDS];
    CRGB output[NUM_LEDS] = {CRGB::Black};

    for (int i = 0; i < NUM_LEDS; i++) {
        input[i] = CRGB(i % 256, (i * 2) % 256, (i * 3) % 256);
    }

    fl::XYMap addressing = fl::XYMap::constructSerpentine(NUM_LEDS, 1);
    addressing.mapPixels(input, output);

    for (int i = 0; i < NUM_LEDS; i++) {
        FL_CHECK_EQ(output[i].r, input[i].r);
        FL_CHECK_EQ(output[i].g, input[i].g);
        FL_CHECK_EQ(output[i].b, input[i].b);
    }
}

FL_TEST_CASE("Edge case: Tall strip (many rows, 1 column)") {
    const int NUM_LEDS = 16;
    CRGB input[NUM_LEDS];
    CRGB output[NUM_LEDS] = {CRGB::Black};

    for (int i = 0; i < NUM_LEDS; i++) {
        input[i] = CRGB(i * 16, i * 8, i * 4);
    }

    fl::XYMap addressing = fl::XYMap::constructSerpentine(1, NUM_LEDS);
    addressing.mapPixels(input, output);

    for (int i = 0; i < NUM_LEDS; i++) {
        FL_CHECK_EQ(output[i].r, input[i].r);
    }
}

FL_TEST_CASE("Corner case: All zeros buffer with addressing") {
    CRGB input[4] = {CRGB::Black};
    CRGB output[4] = {CRGB::Red};

    fl::XYMap addressing = fl::XYMap::constructSerpentine(2, 2);
    addressing.mapPixels(input, output);

    for (int i = 0; i < 4; i++) {
        FL_CHECK(output[i] == CRGB::Black);
    }
}

FL_TEST_CASE("Corner case: All max brightness buffer with addressing") {
    CRGB input[4] = {CRGB::White};
    CRGB output[4] = {CRGB::Black};

    fl::XYMap addressing = fl::XYMap::constructRectangularGrid(2, 2);
    addressing.mapPixels(input, output);

    for (int i = 0; i < 4; i++) {
        FL_CHECK_EQ(output[i].r, 255);
        FL_CHECK_EQ(output[i].g, 255);
        FL_CHECK_EQ(output[i].b, 255);
    }
}

FL_TEST_CASE("ScreenMap copy preserves addressing mode across channels") {
    fl::XYMap original = fl::XYMap::constructSerpentine(4, 4);
    fl::ScreenMap screenmap1 = original.toScreenMap();

    fl::ScreenMap screenmap2 = screenmap1;

    FL_CHECK(screenmap1.hasSourceXYMap());
    FL_CHECK(screenmap2.hasSourceXYMap());

    const XYMap* src1 = screenmap1.getSourceXYMapPtr().get();
    const XYMap* src2 = screenmap2.getSourceXYMapPtr().get();

    FL_CHECK_NE(src1, nullptr);
    FL_CHECK_NE(src2, nullptr);
    FL_CHECK_EQ(src1->getWidth(), src2->getWidth());
    FL_CHECK_EQ(src1->getHeight(), src2->getHeight());
}

FL_TEST_CASE("XYMap addressing with GRB color order") {
    CRGB input[4] = {
        CRGB(255, 0, 0),      // Stored as RGB: Red
        CRGB(0, 255, 0),      // Green
        CRGB(0, 0, 255),      // Blue
        CRGB(255, 255, 0),    // Yellow
    };

    CRGB output[4] = {CRGB::Black};

    fl::XYMap serpentine = fl::XYMap::constructSerpentine(2, 2);
    serpentine.mapPixels(input, output);

    // Serpentine order: (0,0)→0, (1,0)→1, (0,1)→3, (1,1)→2
    FL_CHECK_EQ(output[0].r, 255);  // input[0] Red
    FL_CHECK_EQ(output[1].g, 255);  // input[1] Green
    FL_CHECK_EQ(output[2].r, 255);  // input[3] Yellow
    FL_CHECK_EQ(output[3].b, 255);  // input[2] Blue
}

FL_TEST_CASE("XYMap addressing with BGR color order") {
    CRGB input[4] = {
        CRGB(100, 150, 200),
        CRGB(50, 75, 100),
        CRGB(25, 50, 75),
        CRGB(200, 150, 100),
    };

    CRGB output[4] = {CRGB::Black};

    fl::XYMap rectangular = fl::XYMap::constructRectangularGrid(2, 2);
    rectangular.mapPixels(input, output);

    // Rectangular is identity
    for (int i = 0; i < 4; i++) {
        FL_CHECK_EQ(output[i].r, input[i].r);
        FL_CHECK_EQ(output[i].g, input[i].g);
        FL_CHECK_EQ(output[i].b, input[i].b);
    }
}

FL_TEST_CASE("XYMap addressing with RBG color order") {
    CRGB input[2] = {
        CRGB(100, 50, 150),
        CRGB(200, 75, 125),
    };

    CRGB output[2] = {CRGB::Black};

    fl::XYMap addressing = fl::XYMap::constructSerpentine(2, 1);
    addressing.mapPixels(input, output);

    // 1 row, identity
    FL_CHECK_EQ(output[0].r, 100);
    FL_CHECK_EQ(output[0].g, 50);
    FL_CHECK_EQ(output[0].b, 150);
    FL_CHECK_EQ(output[1].r, 200);
}

FL_TEST_CASE("XYMap addressing with BRG color order") {
    CRGB input[9];
    for (int i = 0; i < 9; i++) {
        input[i] = CRGB(i * 10, i * 20, i * 30);
    }

    CRGB output[9] = {CRGB::Black};

    fl::XYMap addressing = fl::XYMap::constructRectangularGrid(3, 3);
    addressing.mapPixels(input, output);

    for (int i = 0; i < 9; i++) {
        FL_CHECK_EQ(output[i].r, input[i].r);
    }
}

FL_TEST_CASE("XYMap addressing with GBR color order (another permutation)") {
    CRGB input[6] = {
        CRGB(1, 2, 3),
        CRGB(4, 5, 6),
        CRGB(7, 8, 9),
        CRGB(10, 11, 12),
        CRGB(13, 14, 15),
        CRGB(16, 17, 18),
    };

    CRGB output[6] = {CRGB::Black};

    fl::XYMap addressing = fl::XYMap::constructSerpentine(3, 2);
    addressing.mapPixels(input, output);

    // Row 0 normal: 0, 1, 2
    // Row 1 reversed: 5, 4, 3
    FL_CHECK_EQ(output[0].r, 1);
    FL_CHECK_EQ(output[1].r, 4);
    FL_CHECK_EQ(output[5].r, 10);
}
