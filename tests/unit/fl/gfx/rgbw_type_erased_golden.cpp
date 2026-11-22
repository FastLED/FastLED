/// @file tests/fl/gfx/rgbw_type_erased_golden.cpp
/// @brief The type-erased RGBW/RGBWW path in PixelIterator (#4795) must
/// produce exactly the bytes of PixelController's own templated loaders,
/// for every colour order, W placement and mode, including an iterator
/// re-order on top of a PixelController<RGB> (PixelIteratorAny's shape).

#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbww.h"
#include "fl/stl/int.h"
#include "pixel_controller.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

CRGB kLeds[6] = {CRGB(10, 200, 30), CRGB(255, 255, 255), CRGB(0, 0, 0),
                 CRGB(128, 64, 32), CRGB(1, 2, 3), CRGB(250, 5, 120)};

ColorAdjustment adjustment() {
    ColorAdjustment adj = ColorAdjustment::noAdjustment();
    adj.premixed = CRGB(230, 180, 90);  // non-trivial premixed scale
    return adj;
}

template <EOrder O>
void checkRgbwOrder(const Rgbw& rgbw) {
    PixelController<O> reference(kLeds, 6, adjustment(), DISABLE_DITHER);
    PixelController<O> source(kLeds, 6, adjustment(), DISABLE_DITHER);
    PixelIterator it(&source, rgbw);
    // Same bytes through PixelController<RGB> + iterator re-order.
    PixelController<RGB> rgbSource(kLeds, 6, adjustment(), DISABLE_DITHER);
    PixelIterator reordered(&rgbSource, rgbw);
    reordered.setColorOrder(O);
    for (int i = 0; i < 6; ++i) {
        u8 e[4], a[4], r[4];
        reference.loadAndScaleRGBW(rgbw, &e[0], &e[1], &e[2], &e[3]);
        it.loadAndScaleRGBW(&a[0], &a[1], &a[2], &a[3]);
        reordered.loadAndScaleRGBW(&r[0], &r[1], &r[2], &r[3]);
        for (int k = 0; k < 4; ++k) {
            FL_CHECK_EQ(static_cast<int>(a[k]), static_cast<int>(e[k]));
            FL_CHECK_EQ(static_cast<int>(r[k]), static_cast<int>(e[k]));
        }
        reference.advanceData();
        it.advanceData();
        reordered.advanceData();
    }
}

template <EOrder O>
void checkRgbwwOrder(const Rgbww& rgbww) {
    PixelController<O> reference(kLeds, 6, adjustment(), DISABLE_DITHER);
    PixelController<O> source(kLeds, 6, adjustment(), DISABLE_DITHER);
    PixelIterator it(&source, RgbwInvalid::value(), rgbww);
    PixelController<RGB> rgbSource(kLeds, 6, adjustment(), DISABLE_DITHER);
    PixelIterator reordered(&rgbSource, RgbwInvalid::value(), rgbww);
    reordered.setColorOrder(O);
    for (int i = 0; i < 6; ++i) {
        u8 e[5], a[5], r[5];
        reference.loadAndScaleRGBWW(rgbww, &e[0], &e[1], &e[2], &e[3], &e[4]);
        it.loadAndScaleRGBWW(&a[0], &a[1], &a[2], &a[3], &a[4]);
        reordered.loadAndScaleRGBWW(&r[0], &r[1], &r[2], &r[3], &r[4]);
        for (int k = 0; k < 5; ++k) {
            FL_CHECK_EQ(static_cast<int>(a[k]), static_cast<int>(e[k]));
            FL_CHECK_EQ(static_cast<int>(r[k]), static_cast<int>(e[k]));
        }
        reference.advanceData();
        it.advanceData();
        reordered.advanceData();
    }
}

}  // namespace

FL_TEST_CASE("type-erased RGBW matches PixelController for all orders/modes/placements (#4795)") {
    const RGBW_MODE modes[] = {RGBW_MODE::kRGBWExactColors, RGBW_MODE::kRGBWBoostedWhite,
                               RGBW_MODE::kRGBWMaxBrightness, RGBW_MODE::kRGBWNullWhitePixel,
                               RGBW_MODE::kRGBWColorimetric, RGBW_MODE::kRGBWColorimetricBoosted};
    const EOrderW placements[] = {EOrderW::W0, EOrderW::W1, EOrderW::W2, EOrderW::W3};
    for (RGBW_MODE mode : modes) {
        for (EOrderW placement : placements) {
            const Rgbw rgbw(kRGBWDefaultColorTemp, mode, placement);
            checkRgbwOrder<RGB>(rgbw);
            checkRgbwOrder<RBG>(rgbw);
            checkRgbwOrder<GRB>(rgbw);
            checkRgbwOrder<GBR>(rgbw);
            checkRgbwOrder<BRG>(rgbw);
            checkRgbwOrder<BGR>(rgbw);
        }
    }
}

FL_TEST_CASE("type-erased RGBWW matches PixelController for all orders/placements (#4795)") {
    const EOrderWW placements[] = {EOrderWW::WwWcEnd, EOrderWW::WcWwEnd,
                                   EOrderWW::WwWcStart, EOrderWW::WcWwStart};
    const RGBWW_MODE modes[] = {RGBWW_MODE::kRGBWWColorimetric,
                                RGBWW_MODE::kRGBWWColorimetricBoosted,
                                RGBWW_MODE::kRGBWWInvalid};
    for (RGBWW_MODE mode : modes) {
        for (EOrderWW placement : placements) {
            const Rgbww rgbww(kRGBWWDefaultWarmCct, kRGBWWDefaultCoolCct, mode, placement);
            checkRgbwwOrder<RGB>(rgbww);
            checkRgbwwOrder<RBG>(rgbww);
            checkRgbwwOrder<GRB>(rgbww);
            checkRgbwwOrder<GBR>(rgbww);
            checkRgbwwOrder<BRG>(rgbww);
            checkRgbwwOrder<BGR>(rgbww);
        }
    }
}

FL_TEST_CASE("inactive Rgbw takes the null-white fallback with the old bytes (#4795)") {
    // An inactive Rgbw never caches a conversion, so this exercises the
    // fallback regardless of other active Rgbw values in the process.
    const EOrderW placements[] = {EOrderW::W0, EOrderW::W1, EOrderW::W2, EOrderW::W3};
    for (EOrderW placement : placements) {
        Rgbw invalid = RgbwInvalid::value();
        invalid.w_placement = placement;
        checkRgbwOrder<RGB>(invalid);
        checkRgbwOrder<GRB>(invalid);
        checkRgbwOrder<BGR>(invalid);
    }
}

}  // FL_TEST_FILE
