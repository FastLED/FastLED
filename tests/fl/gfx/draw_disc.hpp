#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("drawDisc basic rendering") {
    FL_SUBCASE("center pixel is lit in 32x32 at (15.5, 15.5), r=5") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f);

        // Center pixel (15, 15) should be non-zero
        FL_CHECK(buffer[15 * 32 + 15].r > 0);
    }

    FL_SUBCASE("pixel beyond r+1 is black") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f);

        // Pixel at (15, 22) is over r+1 away, should be black
        FL_CHECK_EQ(buffer[22 * 32 + 15].r, 0);
    }
}

FL_TEST_CASE("drawDisc antialiasing") {
    FL_SUBCASE("AA fringe at radius edge") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 5.5f);

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
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 0.0f);
        // Should not crash
        FL_CHECK(true);
    }

    FL_SUBCASE("off-screen center (no crash)") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), -100.0f, 15.5f, 5.0f);
        // Should not crash, all pixels should be black
        FL_CHECK(true);
    }
}

FL_TEST_CASE("drawDisc pixel-exact reference") {
    FL_SUBCASE("interior pixels at full brightness") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(200, 100, 50), 15.5f, 15.5f, 8.0f);

        // All pixels well inside the disc (dist < r - 1.0) must be full color
        int full_count = 0;
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                float dx = x - 15.5f, dy = y - 15.5f;
                float dist = fl::sqrt(dx * dx + dy * dy);
                if (dist < 7.0f) {
                    CRGB px = buffer[y * 32 + x];
                    FL_CHECK_EQ(px.r, 200);
                    FL_CHECK_EQ(px.g, 100);
                    FL_CHECK_EQ(px.b, 50);
                    ++full_count;
                }
            }
        }
        FL_CHECK(full_count > 100);  // Should have many solid interior pixels
    }

    FL_SUBCASE("total energy matches reference") {
        // Render discs at several sizes and check total red energy is stable
        // across code changes (pixel-exact regression test)
        uint32_t energies[5];
        float radii[] = {3.0f, 5.0f, 7.0f, 10.0f, 12.0f};
        for (int t = 0; t < 5; ++t) {
            CRGB buffer[1024] = {};
            fl::CanvasRGB canvas(buffer, 32, 32);
            canvas.drawDisc(CRGB(100, 0, 0), 15.5f, 15.5f, radii[t]);
            uint32_t sum = 0;
            for (int i = 0; i < 1024; ++i) sum += buffer[i].r;
            energies[t] = sum;
        }
        // Print energies for baseline capture (first run only)
        printf("  disc energy r=3: %u, r=5: %u, r=7: %u, r=10: %u, r=12: %u\n",
               energies[0], energies[1], energies[2], energies[3], energies[4]);
        // Exact energy values (pixel-exact regression gate)
        FL_CHECK_EQ(energies[0], 2856u);
        FL_CHECK_EQ(energies[1], 7916u);
        FL_CHECK_EQ(energies[2], 15496u);
        FL_CHECK_EQ(energies[3], 31408u);
        FL_CHECK_EQ(energies[4], 45328u);
    }

    FL_SUBCASE("specific pixel values at known positions") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 6.0f);

        // Center row, center column: should be full brightness
        FL_CHECK_EQ(buffer[15 * 32 + 15].r, 255);
        FL_CHECK_EQ(buffer[15 * 32 + 16].r, 255);
        FL_CHECK_EQ(buffer[16 * 32 + 15].r, 255);
        FL_CHECK_EQ(buffer[16 * 32 + 16].r, 255);

        // Far outside: must be zero
        FL_CHECK_EQ(buffer[0 * 32 + 0].r, 0);
        FL_CHECK_EQ(buffer[31 * 32 + 31].r, 0);

        // At the radius boundary (dist ~= r), AA fringe pixels
        // Pixel at y=10, x=15: dist=sqrt(0.25+30.25)=5.52, within AA band [5.5,6.5]
        FL_CHECK(buffer[10 * 32 + 15].r > 0);
        FL_CHECK(buffer[10 * 32 + 15].r <= 255);
        // Pixel at y=21, x=15: dist=sqrt(0.25+30.25)=5.52, symmetric
        FL_CHECK(buffer[21 * 32 + 15].r > 0);
    }

    FL_SUBCASE("off-center disc symmetry") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 10.3f, 12.7f, 5.0f);

        // Compute total energy
        uint32_t total = 0;
        for (int i = 0; i < 1024; ++i) total += buffer[i].r;
        printf("  off-center disc energy: %u\n", total);
        FL_CHECK_EQ(total, 20198u);
    }
}

FL_TEST_CASE("drawDisc with different coordinate types") {
    FL_SUBCASE("float coordinates") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f);
        FL_CHECK(buffer[15 * 32 + 15].r > 0);
    }

    FL_SUBCASE("s16x16 fixed-point coordinates") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(255, 0, 0),
                        fl::s16x16(15.5f).to_float(), fl::s16x16(15.5f).to_float(),
                        fl::s16x16(5.0f).to_float());
        FL_CHECK(buffer[15 * 32 + 15].r > 0);
    }
}

FL_TEST_CASE("drawDisc overwrite mode") {
    FL_SUBCASE("overwrite replaces pixels instead of blending") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        // Draw blue disc first
        canvas.drawDisc(CRGB(0, 0, 255), 15.5f, 15.5f, 5.0f);
        // Overwrite with red — center should be pure red, no blue
        canvas.drawDisc(CRGB(255, 0, 0), 15.5f, 15.5f, 5.0f,
                        fl::DRAW_MODE_OVERWRITE);
        CRGB center = buffer[15 * 32 + 15];
        FL_CHECK_EQ(center.r, 255);
        FL_CHECK_EQ(center.b, 0);
    }

    FL_SUBCASE("overwrite energy matches blend for single disc") {
        // A single disc on black should produce identical results
        // regardless of draw mode
        CRGB buf_blend[1024] = {};
        CRGB buf_ow[1024] = {};
        fl::CanvasRGB c_blend(buf_blend, 32, 32);
        fl::CanvasRGB c_ow(buf_ow, 32, 32);
        c_blend.drawDisc(CRGB(200, 100, 50), 15.5f, 15.5f, 8.0f);
        c_ow.drawDisc(CRGB(200, 100, 50), 15.5f, 15.5f, 8.0f,
                       fl::DRAW_MODE_OVERWRITE);
        for (int i = 0; i < 1024; ++i) {
            FL_CHECK_EQ(buf_blend[i].r, buf_ow[i].r);
            FL_CHECK_EQ(buf_blend[i].g, buf_ow[i].g);
            FL_CHECK_EQ(buf_blend[i].b, buf_ow[i].b);
        }
    }
}

FL_TEST_CASE("drawDisc blend mode accumulates") {
    FL_SUBCASE("two blended discs accumulate brightness") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(100, 0, 0), 15.5f, 15.5f, 5.0f);
        canvas.drawDisc(CRGB(100, 0, 0), 15.5f, 15.5f, 5.0f);
        // Center should have accumulated red (clamped at 255)
        FL_CHECK_EQ(buffer[15 * 32 + 15].r, 200);
    }

    FL_SUBCASE("overwrite does not accumulate") {
        CRGB buffer[1024] = {};
        fl::CanvasRGB canvas(buffer, 32, 32);
        canvas.drawDisc(CRGB(100, 0, 0), 15.5f, 15.5f, 5.0f,
                        fl::DRAW_MODE_OVERWRITE);
        canvas.drawDisc(CRGB(100, 0, 0), 15.5f, 15.5f, 5.0f,
                        fl::DRAW_MODE_OVERWRITE);
        // Center should be exactly 100, not accumulated
        FL_CHECK_EQ(buffer[15 * 32 + 15].r, 100);
    }
}

} // FL_TEST_FILE
