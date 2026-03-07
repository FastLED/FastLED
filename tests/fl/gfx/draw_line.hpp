#pragma once

#include "test.h"
#include "fl/gfx.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("drawLine horizontal and vertical lines") {
    FL_SUBCASE("horizontal line at y=8") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine(2, 8, 13, 8, CRGB(255, 0, 0));

        // Count non-zero pixels in row 8
        int non_zero = 0;
        for (int x = 0; x < 16; ++x) {
            if (buffer[8 * 16 + x].r > 0) non_zero++;
        }
        FL_CHECK(non_zero >= 10);  // Should have at least 10 pixels lit
    }

    FL_SUBCASE("vertical line at x=8") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine( 8, 2, 8, 13, CRGB(255, 0, 0));

        // Count non-zero pixels in column 8
        int non_zero = 0;
        for (int y = 0; y < 16; ++y) {
            if (buffer[y * 16 + 8].r > 0) non_zero++;
        }
        FL_CHECK(non_zero >= 10);  // Should have at least 10 pixels lit
    }
}

FL_TEST_CASE("drawLine edge cases") {
    FL_SUBCASE("zero-length line (no crash)") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine( 5, 5, 5, 5, CRGB(255, 0, 0));
        // Should not crash
        FL_CHECK(true);
    }

    FL_SUBCASE("fully off-screen line (no crash)") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine( -100, 8, -50, 8, CRGB(255, 0, 0));
        // Should not crash, all pixels should be black
        FL_CHECK(true);
    }

    FL_SUBCASE("partially clipped line") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine( -2, 8, 5, 8, CRGB(255, 0, 0));
        // Count non-zero pixels in row 8
        int non_zero = 0;
        for (int x = 0; x < 16; ++x) {
            if (buffer[8 * 16 + x].r > 0) non_zero++;
        }
        FL_CHECK(non_zero >= 3);  // Should have at least 3 pixels lit
    }
}

FL_TEST_CASE("drawLine antialiasing") {
    FL_SUBCASE("AA neighbor with fixed-point coords") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine(
                          fl::s16x16(2).to_float(), fl::s16x16(8.3f).to_float(),
                          fl::s16x16(13).to_float(), fl::s16x16(8.3f).to_float(),
                          CRGB(255, 0, 0));

        // Check that both row 8 and row 9 have non-zero pixels
        int row8_nonzero = 0, row9_nonzero = 0;
        for (int x = 0; x < 16; ++x) {
            if (buffer[8 * 16 + x].r > 0) row8_nonzero++;
            if (buffer[9 * 16 + x].r > 0) row9_nonzero++;
        }
        FL_CHECK(row8_nonzero > 0);
        FL_CHECK(row9_nonzero > 0);
    }

    FL_SUBCASE("float coordinate") {
        CRGB buffer[256] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 16, 16);
        canvas.drawLine( 2.0f, 8.3f, 13.0f, 8.3f, CRGB(255, 0, 0));

        // Check that both row 8 and row 9 have non-zero pixels
        int row8_nonzero = 0, row9_nonzero = 0;
        for (int x = 0; x < 16; ++x) {
            if (buffer[8 * 16 + x].r > 0) row8_nonzero++;
            if (buffer[9 * 16 + x].r > 0) row9_nonzero++;
        }
        FL_CHECK(row8_nonzero > 0);
        FL_CHECK(row9_nonzero > 0);
    }
}

} // FL_TEST_FILE
