
#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"
#include "fl/math/fixed_point/s16x16.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("drawStrokeLine basic rendering") {
    FL_SUBCASE("center of stroked line is lit") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        // Horizontal line from (2,8) to (13,8) with thickness 3
        canvas.drawStrokeLine(CRGB(255, 0, 0), 2.0f, 8.0f, 13.0f, 8.0f, 3.0f);

        // Center pixel (8, 8) should be non-zero
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
    }

    FL_SUBCASE("fringe pixels have intermediate brightness") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        // Horizontal line from (2,8) to (13,8) with thickness 3
        canvas.drawStrokeLine(CRGB(255, 0, 0), 2.0f, 8.0f, 13.0f, 8.0f, 3.0f);

        // Fringe pixel at edge should have intermediate brightness
        bool found_fringe = false;
        for (int x = 5; x < 12; ++x) {
            uint8_t r_above = buffer[7 * 16 + x].r;
            uint8_t r_below = buffer[9 * 16 + x].r;
            if ((r_above > 0 && r_above < 255) || (r_below > 0 && r_below < 255)) {
                found_fringe = true;
                break;
            }
        }
        FL_CHECK(found_fringe);
    }

    FL_SUBCASE("pixels beyond thickness are black") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        // Horizontal line from (2,8) to (13,8) with thickness 3
        canvas.drawStrokeLine(CRGB(255, 0, 0), 2.0f, 8.0f, 13.0f, 8.0f, 3.0f);

        // Pixel far from line should be black
        FL_CHECK_EQ(buffer[0 * 16 + 8].r, 0);  // y=0, far from line
    }
}

FL_TEST_CASE("drawStrokeLine FLAT caps") {
    FL_SUBCASE("flat cap: pixels at endpoints but outside segment dropped") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        canvas.drawStrokeLine(CRGB(255, 0, 0), 6.0f, 8.0f, 10.0f, 8.0f, 3.0f, fl::LineCap::FLAT);

        // Pixels at line center should be lit
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
        // Pixels outside [6,10] segment but within radius should be black (FLAT cap behavior)
        FL_CHECK_EQ(buffer[8 * 16 + 4].r, 0);  // x=4, outside segment
    }
}

FL_TEST_CASE("drawStrokeLine ROUND caps") {
    FL_SUBCASE("round cap: hemispherical caps at endpoints") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        canvas.drawStrokeLine(CRGB(255, 0, 0), 6.0f, 8.0f, 10.0f, 8.0f, 3.0f, fl::LineCap::ROUND);

        // Pixels at line center should be lit
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
        // Pixels just beyond endpoint but within radius should be lit (ROUND cap)
        FL_CHECK(buffer[8 * 16 + 5].r > 0);
        FL_CHECK(buffer[8 * 16 + 11].r > 0);
    }
}

FL_TEST_CASE("drawStrokeLine edge cases") {
    FL_SUBCASE("zero thickness (no crash)") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        canvas.drawStrokeLine(CRGB(255, 0, 0), 2.0f, 8.0f, 13.0f, 8.0f, 0.0f);
        // Should not crash
        FL_CHECK(true);
    }

    FL_SUBCASE("off-screen line (no crash)") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        canvas.drawStrokeLine(CRGB(255, 0, 0), -50.0f, 8.0f, -40.0f, 8.0f, 3.0f);
        // Should not crash, all pixels should be black
        FL_CHECK(true);
    }
}

FL_TEST_CASE("drawStrokeLine coordinate type templates") {
    FL_SUBCASE("float coordinates work") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        canvas.drawStrokeLine(CRGB(255, 0, 0), 2.0f, 8.0f, 13.0f, 8.0f, 3.0f);

        // Center pixel should be lit
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
    }

    FL_SUBCASE("s16x16 fixed-point coordinates work") {
        CRGB buffer[256] = {};
        fl::CanvasRGB canvas(buffer, 16, 16);

        fl::s16x16 x0(2), y0(8), x1(13), y1(8), thickness(3);
        canvas.drawStrokeLine(CRGB(0, 0, 255), x0, y0, x1, y1, thickness);

        // Center pixel should be lit
        FL_CHECK(buffer[8 * 16 + 8].b > 0);
    }

    FL_SUBCASE("float and s16x16 produce equivalent results") {
        CRGB buffer_float[256] = {};
        CRGB buffer_fixed[256] = {};

        fl::CanvasRGB canvas_float(buffer_float, 16, 16);
        fl::CanvasRGB canvas_fixed(buffer_fixed, 16, 16);

        canvas_float.drawStrokeLine(CRGB(255, 0, 0), 2.0f, 8.0f, 13.0f, 8.0f, 3.0f);

        fl::s16x16 x0(2), y0(8), x1(13), y1(8), thickness(3);
        canvas_fixed.drawStrokeLine(CRGB(255, 0, 0), x0, y0, x1, y1, thickness);

        // Both should light up the same pixels
        int lit_float = 0, lit_fixed = 0;
        for (int i = 0; i < 256; ++i) {
            if (buffer_float[i].r > 0) lit_float++;
            if (buffer_fixed[i].r > 0) lit_fixed++;
        }

        FL_CHECK(lit_float > 0);
        FL_CHECK(lit_fixed > 0);
        FL_CHECK(fl::abs(lit_float - lit_fixed) <= 2);
    }

    FL_SUBCASE("s16x16 with fractional coordinates works") {
        CRGB buffer_fixed[256] = {};
        fl::CanvasRGB canvas_fixed(buffer_fixed, 16, 16);

        fl::s16x16 x0_frac(2.5f), y0_frac(8.0f), x1_frac(13.5f), y1_frac(8.0f), thickness(3);
        canvas_fixed.drawStrokeLine(CRGB(0, 255, 0), x0_frac, y0_frac, x1_frac, y1_frac, thickness);

        int lit_fixed = 0;
        for (int i = 0; i < 256; ++i) {
            if (buffer_fixed[i].g > 0) lit_fixed++;
        }

        FL_CHECK(lit_fixed > 0);
        FL_CHECK(lit_fixed >= 20);
    }
}

} // FL_TEST_FILE
