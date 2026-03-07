#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"
#include "fl/gfx/crgb16.h"

using fl::CRGB16;
using fl::u8x8;

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("Canvas<CRGB16> drawDisc") {
    FL_SUBCASE("center pixel is lit") {
        CRGB16 buffer[1024] = {};
        fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 1024), 32, 32);
        canvas.drawDisc(CRGB16(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f)), 15.5f, 15.5f, 5.0f);
        FL_CHECK(buffer[15 * 32 + 15].r.raw() > 0);
    }

    FL_SUBCASE("pixel beyond radius is black") {
        CRGB16 buffer[1024] = {};
        fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 1024), 32, 32);
        canvas.drawDisc(CRGB16(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f)), 15.5f, 15.5f, 5.0f);
        FL_CHECK(buffer[22 * 32 + 15].r.raw() == 0);
    }

    FL_SUBCASE("AA fringe has intermediate brightness") {
        CRGB16 buffer[1024] = {};
        fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 1024), 32, 32);
        canvas.drawDisc(CRGB16(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f)), 15.5f, 15.5f, 5.5f);

        bool found_aa = false;
        for (int y = 10; y < 21; ++y) {
            for (int x = 10; x < 21; ++x) {
                float dist = fl::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f));
                if (dist > 5.0f && dist < 6.0f) {
                    fl::u16 rv = buffer[y * 32 + x].r.raw();
                    if (rv > 0 && rv < 0xFF00) {
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

FL_TEST_CASE("Canvas<CRGB16> drawLine") {
    FL_SUBCASE("horizontal line lights correct pixels") {
        CRGB16 buffer[256] = {};
        fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 256), 16, 16);
        canvas.drawLine(CRGB16(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f)), 2.0f, 8.0f, 13.0f, 8.0f);

        int non_zero = 0;
        for (int x = 0; x < 16; ++x) {
            if (buffer[8 * 16 + x].r.raw() > 0) non_zero++;
        }
        FL_CHECK(non_zero >= 10);
    }
}

FL_TEST_CASE("Canvas<CRGB16> drawRing") {
    FL_SUBCASE("center hole is transparent") {
        CRGB16 buffer[1024] = {};
        fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 1024), 32, 32);
        canvas.drawRing(CRGB16(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f)), 15.5f, 15.5f, 5.0f, 2.0f);
        FL_CHECK(buffer[15 * 32 + 15].r.raw() == 0);
    }
}

FL_TEST_CASE("Canvas<CRGB16> drawStrokeLine") {
    FL_SUBCASE("center is lit") {
        CRGB16 buffer[256] = {};
        fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 256), 16, 16);
        canvas.drawStrokeLine(CRGB16(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f)), 2.0f, 8.0f, 13.0f, 8.0f, 3.0f);
        FL_CHECK(buffer[8 * 16 + 8].r.raw() > 0);
    }
}

} // FL_TEST_FILE
