/// @file spi_frame_protocol.cpp
/// @brief Golden framing tests for APA102 and SK9822 channel encoders.

#include "test.h"

#include "fl/chipsets/encoders/apa102.h"
#include "fl/chipsets/encoders/sk9822.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

using Pixel = fl::array<u8, 3>;

fl::vector<Pixel> makePixels(size_t count) {
    fl::vector<Pixel> result;
    for (size_t index = 0; index < count; ++index) {
        // Arrays are already in wire BGR order.
        result.push_back(Pixel{{static_cast<u8>(index),
                                static_cast<u8>(index + 1),
                                static_cast<u8>(index + 2)}});
    }
    return result;
}

size_t frameSize(size_t led_count) {
    return 4 + led_count * 4 + 4 * (led_count / 32 + 1);
}

fl::vector<u8> encodeApa102(size_t led_count, u8 brightness) {
    fl::vector<Pixel> input = makePixels(led_count);
    fl::vector<u8> output;
    encodeAPA102(input.begin(), input.end(), fl::back_inserter(output), brightness);
    return output;
}

fl::vector<u8> encodeSk9822(size_t led_count, u8 brightness) {
    fl::vector<Pixel> input = makePixels(led_count);
    fl::vector<u8> output;
    encodeSK9822(input.begin(), input.end(), fl::back_inserter(output), brightness);
    return output;
}

void checkFrame(const fl::vector<u8>& encoded, size_t led_count,
                u8 brightness) {
    FL_REQUIRE_EQ(encoded.size(), frameSize(led_count));
    for (size_t index = 0; index < 4; ++index) {
        FL_CHECK_EQ(encoded[index], 0x00);
    }
    if (led_count != 0) {
        FL_CHECK_EQ(encoded[4], static_cast<u8>(0xE0 | (brightness & 0x1F)));
        FL_CHECK_EQ(encoded[5], 0x00);
        FL_CHECK_EQ(encoded[6], 0x01);
        FL_CHECK_EQ(encoded[7], 0x02);
        const size_t last = 4 + (led_count - 1) * 4;
        FL_CHECK_EQ(encoded[last], static_cast<u8>(0xE0 | (brightness & 0x1F)));
        FL_CHECK_EQ(encoded[last + 1], static_cast<u8>(led_count - 1));
    }
    for (size_t index = 4 + led_count * 4; index < encoded.size(); ++index) {
        FL_CHECK_EQ(encoded[index], 0xFF);
    }
}

}  // namespace

FL_TEST_CASE("APA102 and SK9822 use exact all-ones end clocks at cascade boundaries") {
    const size_t counts[] = {0, 1, 31, 32, 33, 257};
    for (size_t count : counts) {
        const fl::vector<u8> apa = encodeApa102(count, 31);
        const fl::vector<u8> sk = encodeSk9822(count, 31);
        checkFrame(apa, count, 31);
        checkFrame(sk, count, 31);
        FL_REQUIRE_EQ(sk.size(), apa.size());
        for (size_t index = 0; index < apa.size(); ++index) {
            FL_CHECK_EQ(sk[index], apa[index]);
        }
    }
}

FL_TEST_CASE("SPI LED frame brightness clamps without changing BGR payload or end clocks") {
    const u8 brightnesses[] = {0, 1, 31, 32, 255};
    for (u8 brightness : brightnesses) {
        checkFrame(encodeApa102(1, brightness), 1, brightness);
        checkFrame(encodeSk9822(1, brightness), 1, brightness);
    }
}

FL_TEST_CASE("Auto-brightness encoders keep an empty public frame complete") {
    fl::vector<Pixel> empty;
    fl::vector<u8> apa;
    fl::vector<u8> sk;
    encodeAPA102_AutoBrightness(empty.begin(), empty.end(), fl::back_inserter(apa));
    encodeSK9822_AutoBrightness(empty.begin(), empty.end(), fl::back_inserter(sk));
    checkFrame(apa, 0, 31);
    checkFrame(sk, 0, 31);
}

}  // FL_TEST_FILE
