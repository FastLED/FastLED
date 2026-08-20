/// @file tm1812.hpp
/// @brief Unit tests for the TM1812 RGBCCT byte-packing encoder.

#include "fl/chipsets/encoders/tm1812.h"
#include "fl/channels/config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;

namespace test_tm1812 {

fl::array<u8, 5> makeRgbwwPixel(u8 r, u8 g, u8 b, u8 ww, u8 cw) {
    return {r, g, b, ww, cw};
}

FL_TEST_CASE("TM1812 RGBWW - empty input emits no chip frames") {
    fl::vector<fl::array<u8, 5>> pixels;
    fl::vector<u8> output;

    encodeTM1812_RGBWW(pixels.begin(), pixels.end(),
                       fl::back_inserter(output));

    FL_CHECK(output.empty());
}

FL_TEST_CASE("TM1812 RGBWW - timing trait selects the packing encoder") {
    FL_CHECK_EQ(TIMING_TM1812_RGBWW_800KHZ::T1, 400);
    FL_CHECK_EQ(TIMING_TM1812_RGBWW_800KHZ::T2, 450);
    FL_CHECK_EQ(TIMING_TM1812_RGBWW_800KHZ::T3, 400);
    FL_CHECK_EQ(TIMING_TM1812_RGBWW_800KHZ::RESET, 24);
    FL_CHECK(encoder_for<TIMING_TM1812_RGBWW_800KHZ>() ==
             ClocklessEncoder::CLOCKLESS_ENCODER_TM1812_RGBWW);

    const auto chipset =
        makeClockless<TIMING_TM1812_RGBWW_800KHZ>(3);
    FL_CHECK(chipset.encoder ==
             ClocklessEncoder::CLOCKLESS_ENCODER_TM1812_RGBWW);
}

FL_TEST_CASE("TM1812 RGBWW - two pixels fill ten channels and pad two") {
    fl::vector<fl::array<u8, 5>> pixels;
    pixels.push_back(makeRgbwwPixel(1, 2, 3, 4, 5));
    pixels.push_back(makeRgbwwPixel(6, 7, 8, 9, 10));
    fl::vector<u8> output;

    encodeTM1812_RGBWW(pixels.begin(), pixels.end(),
                       fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 12);
    for (fl::size i = 0; i < 10; ++i) {
        FL_CHECK_EQ(output[i], static_cast<u8>(i + 1));
    }
    FL_CHECK_EQ(output[10], 0);
    FL_CHECK_EQ(output[11], 0);
}

FL_TEST_CASE("TM1812 RGBWW - odd pixel count completes the final chip frame") {
    fl::vector<fl::array<u8, 5>> pixels;
    pixels.push_back(makeRgbwwPixel(11, 12, 13, 14, 15));
    fl::vector<u8> output;

    encodeTM1812_RGBWW(pixels.begin(), pixels.end(),
                       fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 12);
    for (fl::size i = 0; i < 5; ++i) {
        FL_CHECK_EQ(output[i], static_cast<u8>(i + 11));
    }
    for (fl::size i = 5; i < 12; ++i) {
        FL_CHECK_EQ(output[i], 0);
    }
}

FL_TEST_CASE("TM1812 RGBWW - chip framing repeats for later pixels") {
    fl::vector<fl::array<u8, 5>> pixels;
    pixels.push_back(makeRgbwwPixel(1, 2, 3, 4, 5));
    pixels.push_back(makeRgbwwPixel(6, 7, 8, 9, 10));
    pixels.push_back(makeRgbwwPixel(21, 22, 23, 24, 25));
    fl::vector<u8> output;

    encodeTM1812_RGBWW(pixels.begin(), pixels.end(),
                       fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 24);
    FL_CHECK_EQ(output[12], 21);
    FL_CHECK_EQ(output[16], 25);
    for (fl::size i = 17; i < 24; ++i) {
        FL_CHECK_EQ(output[i], 0);
    }
}

} // namespace test_tm1812
