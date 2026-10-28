/// @file i2s_ws2812_codec.cpp
/// @brief Host-side tests for the WS2812-over-I2S protocol codec (#4943).
///
/// The codec is pure integer math with no SDK dependency, so the encoder and
/// the RX-side decoder are verified on the host. The on-device loopback
/// self-test feeds the decoder the samples the I2S engine actually emitted.

#include "platforms/esp/8266/i2s_ws2812_codec.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::esp8266;

namespace {

/// Only the two legal nibbles may appear, and each stands for 1 or 2 high
/// slots (the WS2812 T0H / T1H windows).
void checkNibble(u8 nibble, u8 expectedHighSlots) {
    FL_CHECK_EQ(i2sHighSlots(nibble), expectedHighSlots);
    // The high slots are the four-slot code's leading bits; the rest are low.
    FL_CHECK((nibble & 0x8) != 0);
}

}  // namespace

FL_TEST_CASE("I2S WS2812 codec - nibble codes and high-slot runs") {
    checkNibble(i2sNibble(false), 1);
    checkNibble(i2sNibble(true), 2);
    FL_CHECK_EQ(i2sNibble(false), 0x8);
    FL_CHECK_EQ(i2sNibble(true), 0xC);
}

FL_TEST_CASE("I2S WS2812 codec - every byte round-trips through encode/decode") {
    for (u32 b = 0; b < 256; ++b) {
        u16 samples[kI2sSamplesPerByte];
        encodeByte(static_cast<u8>(b), samples);
        FL_CHECK_EQ(decodeByte(samples[0], samples[1]), static_cast<u8>(b));
    }
}

FL_TEST_CASE("I2S WS2812 codec - MSB first within a sample and across a byte") {
    // 0xF0 = 1111 0000b: first sample is all '1' nibbles, second all '0'.
    u16 samples[2];
    encodeByte(0xF0, samples);
    FL_CHECK_EQ(samples[0], 0xCCCC);
    FL_CHECK_EQ(samples[1], 0x8888);

    // 0x80 = 1000 0000b: first sample = '1' '0' '0' '0'.
    encodeByte(0x80, samples);
    FL_CHECK_EQ(samples[0], 0xC888);
    FL_CHECK_EQ(samples[1], 0x8888);
}

FL_TEST_CASE("I2S WS2812 codec - sample count matches the frame") {
    FL_CHECK_EQ(kI2sBitsPerSample, static_cast<u8>(4));
    FL_CHECK_EQ(i2sSamplesForBytes(0), 0u);
    FL_CHECK_EQ(i2sSamplesForBytes(3), 6u);
    FL_CHECK_EQ(i2sSamplesForBytes(300), 600u);
}

}  // namespace
