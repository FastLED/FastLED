/// @file tm1908.hpp
/// @brief Unit tests for the TM1908 command-prefix encoder.

#include "FastLED.h"
#include "fl/channels/config.h"
#include "fl/chipsets/encoders/tm1908.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;

namespace test_tm1908 {

FL_TEST_CASE("TM1908 - timing and encoder match the vendor protocol") {
    FL_CHECK_EQ(TIMING_TM1908::T1, 240);
    FL_CHECK_EQ(TIMING_TM1908::T2, 240);
    FL_CHECK_EQ(TIMING_TM1908::T3, 350);
    FL_CHECK_EQ(TIMING_TM1908::RESET, 80);
    FL_CHECK(encoder_for<TIMING_TM1908>() ==
             ClocklessEncoder::CLOCKLESS_ENCODER_TM1908);

    const auto chipset = makeClockless<TIMING_TM1908>(3);
    FL_CHECK(chipset.encoder == ClocklessEncoder::CLOCKLESS_ENCODER_TM1908);
}

FL_TEST_CASE("TM1908 - frame contains mode, current, then RGB data") {
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back({0x12, 0x34, 0x56});
    pixels.push_back({0x78, 0x9a, 0xbc});
    fl::vector<u8> output;

    encodeTM1908(pixels.begin(), pixels.end(), fl::back_inserter(output));

    const u8 expected[] = {
        0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f,
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc,
    };
    FL_REQUIRE_EQ(output.size(), static_cast<fl::size>(15));
    for (fl::size i = 0; i < output.size(); ++i) {
        FL_CHECK_EQ(output[i], expected[i]);
    }
}

FL_TEST_CASE("TM1908 - public controller is available") {
    TM1908<3, RGB> controller;
    FL_CHECK(sizeof(controller) > 0);
}

} // namespace test_tm1908
