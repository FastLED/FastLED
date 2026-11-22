
#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("drawRing basic rendering") {
    FL_SUBCASE("center hole is transparent in ring") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f, 2.0f);

        // Center pixel (15, 15) should be black (inside hole)
        FL_CHECK_EQ(buffer[15 * 32 + 15].r, 0);
    }

    FL_SUBCASE("band is lit at r+thickness/2") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f, 2.0f);

        // Pixel at r + thickness/2 from center should be non-zero
        bool found_lit = false;
        for (int y = 10; y < 21; ++y) {
            for (int x = 10; x < 21; ++x) {
                float dist = fl::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f));
                if (dist > 5.5f && dist < 6.5f && buffer[y * 32 + x].r > 0) {
                    found_lit = true;
                    break;
                }
            }
            if (found_lit) break;
        }
        FL_CHECK(found_lit);
    }

    FL_SUBCASE("outside ring is black") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f, 2.0f);

        // Pixel far outside the ring should be black
        FL_CHECK_EQ(buffer[25 * 32 + 15].r, 0);
    }
}

FL_TEST_CASE("drawRing antialiasing") {
    FL_SUBCASE("inner AA fringe") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f, 2.0f);

        // Find a pixel at approximately r from center - should have intermediate brightness
        bool found_aa = false;
        for (int y = 10; y < 21; ++y) {
            for (int x = 10; x < 21; ++x) {
                float dist = fl::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f));
                if (dist > 4.5f && dist < 5.5f) {
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

    FL_SUBCASE("outer AA fringe") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f, 2.0f);

        // Find a pixel at approximately r+thickness from center
        bool found_aa = false;
        for (int y = 10; y < 21; ++y) {
            for (int x = 10; x < 21; ++x) {
                float dist = fl::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f));
                if (dist > 6.5f && dist < 7.5f) {
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

FL_TEST_CASE("drawRing edge cases") {
    FL_SUBCASE("zero thickness (no crash)") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f, 0.0f);
        // Should not crash
        FL_CHECK(true);
    }

    FL_SUBCASE("off-screen center (no crash)") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);

        canvas.drawRing(CRGB(255, 0, 0), -100.0f, 15.5f, 5.0f, 2.0f);
        // Should not crash, all pixels should be black
        FL_CHECK(true);
    }
}

} // FL_TEST_FILE
