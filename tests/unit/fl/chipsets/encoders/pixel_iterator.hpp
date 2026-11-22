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
#include "fl/gfx/pixel_iterator_any.h"
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

// FastLED#5399: the channel API (`FastLED.add()`) encodes plain RGB through
// `PixelIteratorAny` -> `PixelIterator::writeWS2812()`. That encode must
// produce exactly the bytes of the per-pixel adapter path
// (`makeScaledPixelRangeRGB` + `encodeWS2812_RGB`) for every colour order,
// brightness scale, dither mode and strip length, including runs that end
// mid-chunk.
namespace test_pixel_iterator_bulk_rgb {

namespace detail_bulk_rgb {

template <typename PixelControllerT>
void checkRgbBulkMatchesPerPixel(int numLeds, EOrder iteratorOrder,
                                 u8 brightness, EDitherMode dither) {
    CRGB leds[80];
    for (int i = 0; i < numLeds; ++i) {
        leds[i] = CRGB(static_cast<u8>(i * 7 + 1), static_cast<u8>(255 - i * 3),
                       static_cast<u8>(i * 13 + 3));
    }
    // Colour correction folded into `premixed`, then scaled by brightness,
    // as the controller builds it for a frame.
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(scale8(255, brightness), scale8(176, brightness),
                               scale8(240, brightness));

    PixelControllerT base(leds, numLeds, adjustment, dither);
    PixelControllerT forAdapter(base);
    PixelControllerT forBulk(base);

    // The per-pixel path: one ScaledPixelIteratorRGB::advance() per pixel.
    fl::vector<u8> expected;
    expected.push_back(0xA5);  // appends after existing bytes
    {
        PixelIterator it(&forAdapter, RgbwInvalid(), RgbwwInvalid());
        it.setColorOrder(iteratorOrder);
        auto range = makeScaledPixelRangeRGB(&it);
        encodeWS2812_RGB(range.first, range.second, fl::back_inserter(expected));
    }

    fl::vector<u8> actual;
    actual.push_back(0xA5);
    {
        PixelIterator it(&forBulk, RgbwInvalid(), RgbwwInvalid());
        it.setColorOrder(iteratorOrder);
        it.writeWS2812(&actual);
    }

    FL_REQUIRE_EQ(expected.size(), 1u + static_cast<fl::size>(numLeds) * 3u);
    FL_CHECK_EQ(actual, expected);
}

}  // namespace detail_bulk_rgb

FL_TEST_CASE("RGB writeWS2812 matches the per-pixel path byte for byte #5399") {
    const u8 savedDitherFrame = fl::detail::gDitherFrame;
    auto restoreDither = fl::make_scope_exit(
        [&]() { fl::detail::gDitherFrame = savedDitherFrame; });

    // 1 and 3: shorter than a run; 33 and 75: across run boundaries, odd.
    const int lengths[] = {1, 3, 32, 33, 75};
    const EOrder orders[] = {RGB, RBG, GRB, GBR, BRG, BGR};
    const u8 brightnesses[] = {255, 128, 37};
    const EDitherMode dithers[] = {DISABLE_DITHER, BINARY_DITHER};
    for (int len : lengths) {
        for (EOrder order : orders) {
            for (u8 brightness : brightnesses) {
                for (EDitherMode dither : dithers) {
                    fl::detail::advanceDitherFrame();
                    // What the channel API builds (PixelIteratorAny's source).
                    detail_bulk_rgb::checkRgbBulkMatchesPerPixel<
                        PixelController<RGB, 1, 0xFFFFFFFF>>(len, order, brightness, dither);
                    // A templated driver's own ordered controller.
                    detail_bulk_rgb::checkRgbBulkMatchesPerPixel<PixelController<GRB>>(
                        len, order, brightness, dither);
                }
            }
        }
    }
}

FL_TEST_CASE("RGB writeWS2812 through PixelIteratorAny matches the per-pixel path #5399") {
    // The channel API's construction: an RGB controller re-ordered by
    // PixelIteratorAny, with dithering on and brightness below full.
    constexpr int kLen = 35;
    CRGB leds[kLen];
    for (int i = 0; i < kLen; ++i) {
        leds[i] = CRGB(static_cast<u8>(i * 5 + 2), static_cast<u8>(i * 9),
                       static_cast<u8>(200 - i));
    }
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(90, 90, 90);
    const EOrder orders[] = {RGB, RBG, GRB, GBR, BRG, BGR};
    for (EOrder order : orders) {
        PixelController<RGB, 1, 0xFFFFFFFF> forAdapter(leds, kLen, adjustment, BINARY_DITHER);
        PixelController<RGB, 1, 0xFFFFFFFF> forBulk(forAdapter);
        fl::vector<u8> expected;
        {
            PixelIteratorAny any(forAdapter, order, RgbwInvalid(), RgbwwInvalid());
            auto range = makeScaledPixelRangeRGB(&any.get());
            encodeWS2812_RGB(range.first, range.second, fl::back_inserter(expected));
        }
        fl::vector<u8> actual;
        {
            PixelIteratorAny any(forBulk, order, RgbwInvalid(), RgbwwInvalid());
            any.get().writeWS2812(&actual);
        }
        FL_REQUIRE_EQ(expected.size(), kLen * 3u);
        FL_CHECK_EQ(actual, expected);
    }
}

}  // namespace test_pixel_iterator_bulk_rgb
