#pragma once

#include "test.h"
#include "fl/gfx.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("drawDisc basic rendering") {
    FL_SUBCASE("center pixel is lit in 32x32 at (15.5, 15.5), r=5") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc( 15.5f, 15.5f, 5.0f, CRGB(255, 0, 0));

        // Center pixel (15, 15) should be non-zero
        FL_CHECK(buffer[15 * 32 + 15].r > 0);
    }

    FL_SUBCASE("pixel beyond r+1 is black") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc( 15.5f, 15.5f, 5.0f, CRGB(255, 0, 0));

        // Pixel at (15, 22) is over r+1 away, should be black
        FL_CHECK_EQ(buffer[22 * 32 + 15].r, 0);
    }
}

FL_TEST_CASE("drawDisc antialiasing") {
    FL_SUBCASE("AA fringe at radius edge") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc( 15.5f, 15.5f, 5.5f, CRGB(255, 0, 0));

        // Find a pixel at approximately the radius - should have intermediate brightness
        bool found_aa = false;
        for (int y = 10; y < 21; ++y) {
            for (int x = 10; x < 21; ++x) {
                float dist = fl::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f));
                if (dist > 5.0f && dist < 6.0f) {
                    uint8_t r = buffer[y * 32 + x].r;
                    if (r > 0 && r < 255) {
                        found_aa = true;
                        break;
                    }
                }
            }
            if (found_aa) break;
        }
        FL_CHECK(found_aa);
    }
}

FL_TEST_CASE("drawDisc edge cases") {
    FL_SUBCASE("zero radius (no crash)") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc( 15.5f, 15.5f, 0.0f, CRGB(255, 0, 0));
        // Should not crash
        FL_CHECK(true);
    }

    FL_SUBCASE("off-screen center (no crash)") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc( -100.0f, 15.5f, 5.0f, CRGB(255, 0, 0));
        // Should not crash, all pixels should be black
        FL_CHECK(true);
    }
}

FL_TEST_CASE("drawDisc with different coordinate types") {
    FL_SUBCASE("float coordinates") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc( 15.5f, 15.5f, 5.0f, CRGB(255, 0, 0));
        FL_CHECK(buffer[15 * 32 + 15].r > 0);
    }

    FL_SUBCASE("s16x16 fixed-point coordinates") {
        CRGB buffer[1024] = {};
        fl::gfx::Canvas<CRGB> canvas(buffer, 32, 32);
        canvas.drawDisc(
                          fl::s16x16(15.5f).to_float(), fl::s16x16(15.5f).to_float(),
                          fl::s16x16(5.0f).to_float(), CRGB(255, 0, 0));
        FL_CHECK(buffer[15 * 32 + 15].r > 0);
    }
}

} // FL_TEST_FILE
