#include "FastLED.h"
#include "test.h"

namespace {

static const TProgmemHSVPalette16 kProgmemHsv16 FL_PROGMEM = {
    0x010203, 0x111213, 0x212223, 0x313233,
    0x414243, 0x515253, 0x616263, 0x717273,
    0x818283, 0x919293, 0xA1A2A3, 0xB1B2B3,
    0xC1C2C3, 0xD1D2D3, 0xE1E2E3, 0xF1F2F3,
};

static const TProgmemHSVPalette32 kProgmemHsv32 FL_PROGMEM = {
    0x000102, 0x030405, 0x060708, 0x090A0B, 0x0C0D0E, 0x0F1011, 0x121314, 0x151617,
    0x18191A, 0x1B1C1D, 0x1E1F20, 0x212223, 0x242526, 0x272829, 0x2A2B2C, 0x2D2E2F,
    0x303132, 0x333435, 0x363738, 0x393A3B, 0x3C3D3E, 0x3F4041, 0x424344, 0x454647,
    0x48494A, 0x4B4C4D, 0x4E4F50, 0x515253, 0x545556, 0x575859, 0x5A5B5C, 0x5D5E5F,
};

static const TProgmemRGBPalette32 kProgmemRgb32 FL_PROGMEM = {
    0x000102, 0x030405, 0x060708, 0x090A0B, 0x0C0D0E, 0x0F1011, 0x121314, 0x151617,
    0x18191A, 0x1B1C1D, 0x1E1F20, 0x212223, 0x242526, 0x272829, 0x2A2B2C, 0x2D2E2F,
    0x303132, 0x333435, 0x363738, 0x393A3B, 0x3C3D3E, 0x3F4041, 0x424344, 0x454647,
    0x48494A, 0x4B4C4D, 0x4E4F50, 0x515253, 0x545556, 0x575859, 0x5A5B5C, 0x5D5E5F,
};

static const TProgmemRGBPalette16 kProgmemRgb16 FL_PROGMEM = {
    0x000000, 0xFFFFFF, 0x000000, 0x000000,
    0x000000, 0x000000, 0x000000, 0x000000,
    0x000000, 0x000000, 0x000000, 0x000000,
    0x000000, 0x000000, 0x000000, 0x800000,
};

CHSV make_hsv(int i) {
    return CHSV(static_cast<fl::u8>(i), static_cast<fl::u8>(100 + i),
                static_cast<fl::u8>(200 - i));
}

CRGB make_rgb(int i) {
    return CRGB(static_cast<fl::u8>(i), static_cast<fl::u8>(2 * i),
                static_cast<fl::u8>(255 - i));
}

void check_hsv_eq(const CHSV &lhs, const CHSV &rhs) {
    FL_CHECK_EQ(lhs.hue, rhs.hue);
    FL_CHECK_EQ(lhs.sat, rhs.sat);
    FL_CHECK_EQ(lhs.val, rhs.val);
}

void check_rgb_eq(const CRGB &lhs, const CRGB &rhs) {
    FL_CHECK_EQ(lhs.red, rhs.red);
    FL_CHECK_EQ(lhs.green, rhs.green);
    FL_CHECK_EQ(lhs.blue, rhs.blue);
}

CRGB downconvert(const fl::CRGB16 &rgb) {
    return CRGB(static_cast<fl::u8>(rgb.r.to_int()),
                static_cast<fl::u8>(rgb.g.to_int()),
                static_cast<fl::u8>(rgb.b.to_int()));
}

} // namespace

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("CHSV palettes upscale consistently") {
    CHSVPalette16 hsv16(
        make_hsv(0), make_hsv(1), make_hsv(2), make_hsv(3),
        make_hsv(4), make_hsv(5), make_hsv(6), make_hsv(7),
        make_hsv(8), make_hsv(9), make_hsv(10), make_hsv(11),
        make_hsv(12), make_hsv(13), make_hsv(14), make_hsv(15));

    FL_SUBCASE("32-entry constructor and assignment repeat 16-entry anchors") {
        CHSVPalette32 from_ctor(hsv16);
        CHSVPalette32 from_assign;
        from_assign = hsv16;

        FL_CHECK(from_ctor == from_assign);
        for (int i = 0; i < 16; ++i) {
            check_hsv_eq(from_ctor[2 * i + 0], hsv16[i]);
            check_hsv_eq(from_ctor[2 * i + 1], hsv16[i]);
        }
    }

    FL_SUBCASE("256-entry constructor and assignment from 32-entry preserve ColorFromPalette semantics") {
        CHSVPalette32 hsv32(hsv16);
        CHSVPalette256 from_ctor(hsv32);
        CHSVPalette256 from_assign;
        from_assign = hsv32;

        FL_CHECK(from_ctor == from_assign);
        for (int i = 0; i < 256; ++i) {
            check_hsv_eq(from_ctor[i], ColorFromPalette(hsv32, static_cast<fl::u8>(i)));
        }
    }
}

FL_TEST_CASE("palette progmem constructors stay aligned with runtime conversions") {
    FL_SUBCASE("CHSV 16-entry progmem can build 32-entry and 256-entry palettes") {
        CHSVPalette16 from_prog16(kProgmemHsv16);
        CHSVPalette32 hsv32(kProgmemHsv16);
        CHSVPalette256 hsv256(kProgmemHsv16);

        for (int i = 0; i < 16; ++i) {
            check_hsv_eq(hsv32[2 * i + 0], from_prog16[i]);
            check_hsv_eq(hsv32[2 * i + 1], from_prog16[i]);
            check_hsv_eq(hsv256[16 * i], from_prog16[i]);
        }
    }

    FL_SUBCASE("CHSV 32-entry progmem can build a 256-entry palette") {
        CHSVPalette32 from_prog32(kProgmemHsv32);
        CHSVPalette256 hsv256(kProgmemHsv32);

        for (int i = 0; i < 256; ++i) {
            check_hsv_eq(hsv256[i], ColorFromPalette(from_prog32, static_cast<fl::u8>(i)));
        }
    }

    FL_SUBCASE("CRGB 32-entry progmem can build a 256-entry palette") {
        CRGBPalette32 from_prog32(kProgmemRgb32);
        CRGBPalette256 rgb256(kProgmemRgb32);

        for (int i = 0; i < 256; ++i) {
            check_rgb_eq(rgb256[i], ColorFromPalette(from_prog32, static_cast<fl::u8>(i)));
        }
    }
}

FL_TEST_CASE("CRGB palettes still support same-size CHSV conversions and 32-to-256 upscaling") {
    CHSVPalette32 hsv32(
        make_hsv(0), make_hsv(1), make_hsv(2), make_hsv(3),
        make_hsv(4), make_hsv(5), make_hsv(6), make_hsv(7),
        make_hsv(8), make_hsv(9), make_hsv(10), make_hsv(11),
        make_hsv(12), make_hsv(13), make_hsv(14), make_hsv(15));

    CRGBPalette32 rgb32_from_hsv(hsv32);
    for (int i = 0; i < 32; ++i) {
        check_rgb_eq(rgb32_from_hsv[i], CRGB(hsv32[i]));
    }

    CRGBPalette32 rgb32(
        make_rgb(0), make_rgb(1), make_rgb(2), make_rgb(3),
        make_rgb(4), make_rgb(5), make_rgb(6), make_rgb(7),
        make_rgb(8), make_rgb(9), make_rgb(10), make_rgb(11),
        make_rgb(12), make_rgb(13), make_rgb(14), make_rgb(15));

    CRGBPalette256 rgb256(rgb32);
    CRGBPalette256 rgb256_assign;
    rgb256_assign = rgb32;

    FL_CHECK(rgb256 == rgb256_assign);
    for (int i = 0; i < 256; ++i) {
        check_rgb_eq(rgb256[i], ColorFromPalette(rgb32, static_cast<fl::u8>(i)));
    }
}

FL_TEST_CASE("palette single-color constructors remain copy-initializable") {
    CHSV hsv_color = make_hsv(7);
    CRGB rgb_color = make_rgb(9);

    CHSVPalette16 hsv_solid = hsv_color;
    CRGBPalette32 rgb_solid = rgb_color;
    CRGBPalette16 rgb_from_hsv = hsv_color;

    for (int i = 0; i < 16; ++i) {
        check_hsv_eq(hsv_solid[i], hsv_color);
        check_rgb_eq(rgb_from_hsv[i], CRGB(hsv_color));
    }

    for (int i = 0; i < 32; ++i) {
        check_rgb_eq(rgb_solid[i], rgb_color);
    }
}

FL_TEST_CASE("ColorFromPaletteHD preserves CRGB16 precision") {
    CRGBPalette16 pal(CRGB::Black);
    pal[0] = CRGB::Black;
    pal[1] = CRGB::White;
    pal[15] = CRGB(128, 0, 0);

    FL_SUBCASE("CRGBPalette16 lookup keeps sub-8-bit interpolation") {
        fl::CRGB16 color = ColorFromPaletteHD(pal, 1, fl::u8x8(1),
                                              LINEARBLEND);

        FL_CHECK_EQ(color.r.to_int(), 0u);
        FL_CHECK_GT(color.r.raw(), 0u);
        FL_CHECK_EQ(color.r.raw(), color.g.raw());
        FL_CHECK_EQ(color.g.raw(), color.b.raw());
    }

    FL_SUBCASE("low brightness scaling keeps fractional channel data") {
        fl::CRGB16 color = ColorFromPaletteHD(
            pal, 0x1000, fl::u8x8::from_raw(1), NOBLEND);

        FL_CHECK_EQ(color.r.to_int(), 0u);
        FL_CHECK_GT(color.r.raw(), 0u);
        FL_CHECK_EQ(color.r.raw(), color.g.raw());
        FL_CHECK_EQ(color.g.raw(), color.b.raw());
    }

    FL_SUBCASE("LINEARBLEND wraps and LINEARBLEND_NOWRAP holds final entry") {
        fl::CRGB16 wrap = ColorFromPaletteHD(pal, 0xF800, fl::u8x8(1),
                                             LINEARBLEND);
        fl::CRGB16 no_wrap = ColorFromPaletteHD(pal, 0xF800, fl::u8x8(1),
                                                LINEARBLEND_NOWRAP);

        FL_CHECK_LT(wrap.r.raw(), no_wrap.r.raw());
        FL_CHECK_EQ(no_wrap.r.raw(), fl::u16(128) << 8);
        FL_CHECK_EQ(no_wrap.g.raw(), 0u);
        FL_CHECK_EQ(no_wrap.b.raw(), 0u);
    }

    FL_SUBCASE("downconversion matches existing extended lookup at anchors") {
        const fl::u16 anchors[] = {0x0000, 0x1000, 0xF000};
        for (fl::u16 index : anchors) {
            CRGB legacy =
                ColorFromPaletteExtended(pal, index, 255, LINEARBLEND);
            CRGB hd = downconvert(
                ColorFromPaletteHD(pal, index, fl::u8x8(1), LINEARBLEND));
            check_rgb_eq(hd, legacy);
        }
    }
}

FL_TEST_CASE("ColorFromPaletteHD supports existing RGB palette families") {
    CRGBPalette16 pal16(CRGB::Black);
    pal16[0] = CRGB(0, 0, 0);
    pal16[1] = CRGB(255, 0, 0);

    CRGBPalette32 pal32(CRGB::Black);
    pal32[0] = CRGB(0, 0, 0);
    pal32[1] = CRGB(0, 255, 0);

    CRGBPalette256 pal256(CRGB::Black);
    pal256[0] = CRGB(0, 0, 0);
    pal256[1] = CRGB(0, 0, 255);

    FL_CHECK_GT(ColorFromPaletteHD(pal16, 1, fl::u8x8(1), LINEARBLEND).r.raw(),
                0u);
    FL_CHECK_GT(ColorFromPaletteHD(pal32, 1, fl::u8x8(1), LINEARBLEND).g.raw(),
                0u);
    FL_CHECK_GT(ColorFromPaletteHD(pal256, 1, fl::u8x8(1), LINEARBLEND).b.raw(),
                0u);

    FL_CHECK_GT(
        ColorFromPaletteHD(kProgmemRgb16, 1, fl::u8x8(1), LINEARBLEND).r.raw(),
        0u);
    FL_CHECK_GT(
        ColorFromPaletteHD(kProgmemRgb32, 0x0800, fl::u8x8(1), LINEARBLEND)
            .r.raw(),
        0u);
}

} // FL_TEST_FILE
