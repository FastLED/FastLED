#pragma once

#include "test.h"
#include "fl/gfx.h"
#include "fl/stl/fixed_point/s16x16.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("drawStrokeLine basic rendering") {
    FL_SUBCASE("center of stroked line is lit") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Horizontal line from (2,8) to (13,8) with thickness 3
        canvas.drawStrokeLine( 2.0f, 8.0f, 13.0f, 8.0f, 3.0f, CRGB(255, 0, 0));

        // Center pixel (8, 8) should be non-zero
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
    }

    FL_SUBCASE("fringe pixels have intermediate brightness") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Horizontal line from (2,8) to (13,8) with thickness 3
        canvas.drawStrokeLine( 2.0f, 8.0f, 13.0f, 8.0f, 3.0f, CRGB(255, 0, 0));

        // Fringe pixel at edge should have intermediate brightness
        // Thickness=3, so radius=1.5, fringe should be around y=8±2
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
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Horizontal line from (2,8) to (13,8) with thickness 3
        canvas.drawStrokeLine( 2.0f, 8.0f, 13.0f, 8.0f, 3.0f, CRGB(255, 0, 0));

        // Pixel far from line should be black
        FL_CHECK_EQ(buffer[0 * 16 + 8].r, 0);  // y=0, far from line
    }
}

FL_TEST_CASE("drawStrokeLine FLAT caps") {
    FL_SUBCASE("flat cap: pixels at endpoints but outside segment dropped") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Horizontal line from (6,8) to (10,8) with thickness 3, FLAT cap
        canvas.drawStrokeLine( 6.0f, 8.0f, 10.0f, 8.0f, 3.0f, CRGB(255, 0, 0), fl::gfx::LineCap::FLAT);

        // Pixels at line center should be lit
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
        // Pixels outside [6,10] segment but within radius should be black (FLAT cap behavior)
        FL_CHECK_EQ(buffer[8 * 16 + 4].r, 0);  // x=4, outside segment
    }
}

FL_TEST_CASE("drawStrokeLine ROUND caps") {
    FL_SUBCASE("round cap: hemispherical caps at endpoints") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Horizontal line from (6,8) to (10,8) with thickness 3, ROUND cap
        canvas.drawStrokeLine( 6.0f, 8.0f, 10.0f, 8.0f, 3.0f, CRGB(255, 0, 0), fl::gfx::LineCap::ROUND);

        // Pixels at line center should be lit
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
        // Pixels just beyond endpoint but within radius should be lit (ROUND cap)
        FL_CHECK(buffer[8 * 16 + 5].r > 0);  // x=5, just beyond x0=6, but within radius
        FL_CHECK(buffer[8 * 16 + 11].r > 0);  // x=11, just beyond x1=10, but within radius
    }
}

FL_TEST_CASE("drawStrokeLine edge cases") {
    FL_SUBCASE("zero thickness (no crash)") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        canvas.drawStrokeLine( 2.0f, 8.0f, 13.0f, 8.0f, 0.0f, CRGB(255, 0, 0));
        // Should not crash
        FL_CHECK(true);
    }

    FL_SUBCASE("off-screen line (no crash)") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        canvas.drawStrokeLine( -50.0f, 8.0f, -40.0f, 8.0f, 3.0f, CRGB(255, 0, 0));
        // Should not crash, all pixels should be black
        FL_CHECK(true);
    }
}

FL_TEST_CASE("drawStrokeLine coordinate type templates") {
    FL_SUBCASE("float coordinates work") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Call with float
        canvas.drawStrokeLine(2.0f, 8.0f, 13.0f, 8.0f, 3.0f, CRGB(255, 0, 0));

        // Center pixel should be lit
        FL_CHECK(buffer[8 * 16 + 8].r > 0);
    }

    FL_SUBCASE("s16x16 fixed-point coordinates work") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);

        // Call with s16x16 fixed-point
        fl::s16x16 x0(2), y0(8), x1(13), y1(8), thickness(3);
        canvas.drawStrokeLine(x0, y0, x1, y1, thickness, CRGB(0, 0, 255));

        // Center pixel should be lit (same position as other tests)
        FL_CHECK(buffer[8 * 16 + 8].b > 0);
    }

    FL_SUBCASE("float and s16x16 produce equivalent results") {
        CRGB buffer_float[256] = {};
        CRGB buffer_fixed[256] = {};

        fl::gfx::Canvas<CRGB> canvas_float(buffer_float, 16, 16);
        fl::gfx::Canvas<CRGB> canvas_fixed(buffer_fixed, 16, 16);

        // Draw same line with float
        canvas_float.drawStrokeLine(2.0f, 8.0f, 13.0f, 8.0f, 3.0f, CRGB(255, 0, 0));

        // Draw same line with s16x16
        fl::s16x16 x0(2), y0(8), x1(13), y1(8), thickness(3);
        canvas_fixed.drawStrokeLine(x0, y0, x1, y1, thickness, CRGB(255, 0, 0));

        // Both should light up the same pixels
        int lit_float = 0, lit_fixed = 0;
        for (int i = 0; i < 256; ++i) {
            if (buffer_float[i].r > 0) lit_float++;
            if (buffer_fixed[i].r > 0) lit_fixed++;
        }

        // Should light up roughly the same number of pixels (allowing small difference for rounding)
        FL_CHECK(lit_float > 0);
        FL_CHECK(lit_fixed > 0);
        FL_CHECK(fl::abs(lit_float - lit_fixed) <= 2);  // Allow 1-2 pixel difference from rounding
    }

    FL_SUBCASE("s16x16 with fractional coordinates works") {
        CRGB buffer_fixed[256] = {};
        fl::gfx::Canvas<CRGB> canvas_fixed(buffer_fixed, 16, 16);

        // Draw line at fractional coordinates (2.5, 8.0) to (13.5, 8.0) with s16x16 sub-pixel precision
        fl::s16x16 x0_frac(2.5f);    // 2.5 with sub-pixel precision
        fl::s16x16 y0_frac(8.0f);    // 8.0
        fl::s16x16 x1_frac(13.5f);   // 13.5 with sub-pixel precision
        fl::s16x16 y1_frac(8.0f);    // 8.0
        fl::s16x16 thickness(3);
        canvas_fixed.drawStrokeLine(x0_frac, y0_frac, x1_frac, y1_frac, thickness, CRGB(0, 255, 0));

        // Count lit pixels
        int lit_fixed = 0;
        for (int i = 0; i < 256; ++i) {
            if (buffer_fixed[i].g > 0) lit_fixed++;
        }

        // Should have lit pixels with fractional coordinates
        FL_CHECK(lit_fixed > 0);
        // Should light up pixels similar to the integer version
        FL_CHECK(lit_fixed >= 20);  // Expect roughly 20+ pixels lit for a ~11 unit wide, 3 thick line
    }
}

} // FL_TEST_FILE
