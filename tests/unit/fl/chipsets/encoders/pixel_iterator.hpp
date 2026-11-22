/// @file pixel_iterator.hpp
/// FastLED#5377: PixelIterator's RGBW encode converts whole runs of pixels
/// (one type-erased load and one conversion call per run). It must produce
/// exactly the bytes of the per-pixel adapter path it replaces, for every
/// RGBW mode, W placement, colour order and dither mode, across run
/// boundaries.

#include "FastLED.h"
#include "fl/channels/dither_frame.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/encoders/pixel_iterator_adapters.h"
#include "fl/chipsets/encoders/ws2812.h"
#include "fl/stl/iterator.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;

namespace test_pixel_iterator_bulk {

namespace detail_bulk {

// More than two 32-pixel runs, ending mid-run.
constexpr int kLeds = 75;

template <EOrder ORDER>
void checkRgbwBulkMatchesPerPixel(RGBW_MODE mode, EOrderW placement,
                                  EOrder iteratorOrder, EDitherMode dither) {
    CRGB leds[kLeds];
    for (int i = 0; i < kLeds; ++i) {
        leds[i] = CRGB(static_cast<u8>(i * 7 + 1), static_cast<u8>(255 - i * 3),
                       static_cast<u8>(i * 13 + 3));
    }
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(200, 97, 31);
    const Rgbw rgbw(kRGBWDefaultColorTemp, mode, placement);

    PixelController<ORDER> base(leds, kLeds, adjustment, dither);
    PixelController<ORDER> forAdapter(base);
    PixelController<ORDER> forBulk(base);

    // The per-pixel path the bulk encode replaces.
    fl::vector<u8> expected;
    expected.push_back(0xA5);  // appends after existing bytes
    {
        PixelIterator it(&forAdapter, rgbw, RgbwwInvalid());
        it.setColorOrder(iteratorOrder);
        auto range = makeScaledPixelRangeRGBW(&it);
        encodeWS2812_RGBW(range.first, range.second, fl::back_inserter(expected));
    }

    fl::vector<u8> actual;
    actual.push_back(0xA5);
    {
        PixelIterator it(&forBulk, rgbw, RgbwwInvalid());
        it.setColorOrder(iteratorOrder);
        it.writeWS2812(&actual);
    }

    FL_REQUIRE_EQ(expected.size(), 1u + kLeds * 4u);
    FL_CHECK_EQ(actual, expected);
}

}  // namespace detail_bulk

FL_TEST_CASE("RGBW bulk encode matches the per-pixel path byte for byte #5377") {
    const u8 savedDitherFrame = fl::detail::gDitherFrame;
    auto restoreDither = fl::make_scope_exit(
        [&]() { fl::detail::gDitherFrame = savedDitherFrame; });

    const RGBW_MODE modes[] = {
        RGBW_MODE::kRGBWExactColors, RGBW_MODE::kRGBWMaxBrightness,
        RGBW_MODE::kRGBWBoostedWhite, RGBW_MODE::kRGBWNullWhitePixel};
    const EOrderW placements[] = {W0, W1, W2, W3};
    const EOrder orders[] = {RGB, GRB, BGR};
    const EDitherMode dithers[] = {DISABLE_DITHER, BINARY_DITHER};
    for (RGBW_MODE mode : modes) {
        for (EOrderW placement : placements) {
            for (EOrder order : orders) {
                for (EDitherMode dither : dithers) {
                    fl::detail::advanceDitherFrame();
                    detail_bulk::checkRgbwBulkMatchesPerPixel<RGB>(mode, placement, order, dither);
                    detail_bulk::checkRgbwBulkMatchesPerPixel<GRB>(mode, placement, order, dither);
                }
            }
        }
    }
}

namespace detail_bulk {

template <EOrder ORDER>
void checkRgbwwBulkMatchesPerPixel(RGBWW_MODE mode, EOrderWW placement,
                                   EOrder iteratorOrder) {
    CRGB leds[kLeds];
    for (int i = 0; i < kLeds; ++i) {
        leds[i] = CRGB(static_cast<u8>(i * 11 + 5), static_cast<u8>(i * 3),
                       static_cast<u8>(250 - i * 2));
    }
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(180, 255, 64);
    const Rgbww rgbww(kRGBWWDefaultWarmCct, kRGBWWDefaultCoolCct, mode, placement);

    PixelController<ORDER> base(leds, kLeds, adjustment, BINARY_DITHER);
    PixelController<ORDER> forAdapter(base);
    PixelController<ORDER> forBulk(base);

    fl::vector<u8> expected;
    {
        PixelIterator it(&forAdapter, RgbwInvalid(), rgbww);
        it.setColorOrder(iteratorOrder);
        auto range = makeScaledPixelRangeRGBWW(&it);
        encodeWS2812_RGBWW(range.first, range.second, fl::back_inserter(expected));
    }
    fl::vector<u8> actual;
    {
        PixelIterator it(&forBulk, RgbwInvalid(), rgbww);
        it.setColorOrder(iteratorOrder);
        it.writeWS2812(&actual);
    }
    FL_REQUIRE_EQ(expected.size(), kLeds * 5u);
    FL_CHECK_EQ(actual, expected);
}

}  // namespace detail_bulk

FL_TEST_CASE("RGBWW bulk encode matches the per-pixel path byte for byte #5377") {
    const RGBWW_MODE modes[] = {RGBWW_MODE::kRGBWWColorimetric,
                                RGBWW_MODE::kRGBWWColorimetricBoosted};
    const EOrderWW placements[] = {EOrderWW::WwWcEnd, EOrderWW::WcWwEnd,
                                   EOrderWW::WwWcStart, EOrderWW::WcWwStart};
    const EOrder orders[] = {RGB, GRB, BRG};
    for (RGBWW_MODE mode : modes) {
        for (EOrderWW placement : placements) {
            for (EOrder order : orders) {
                detail_bulk::checkRgbwwBulkMatchesPerPixel<RGB>(mode, placement, order);
                detail_bulk::checkRgbwwBulkMatchesPerPixel<GRB>(mode, placement, order);
            }
        }
    }
}

FL_TEST_CASE("RGBW bulk encode of an empty strip writes nothing #5377") {
    CRGB leds[1];
    PixelController<RGB> pixels(leds, 0, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    PixelIterator it(&pixels, RgbwDefault(), RgbwwInvalid());
    fl::vector<u8> out;
    it.writeWS2812(&out);
    FL_CHECK(out.empty());
}

}  // namespace test_pixel_iterator_bulk
