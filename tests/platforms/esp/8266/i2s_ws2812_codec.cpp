/// @file i2s_ws2812_codec.cpp
/// @brief Host-side tests for the WS2812-over-I2S protocol codec (#4943).
///
/// The codec is pure integer math with no SDK dependency, so the encoder and
/// the RX-side decoder are verified on the host. The on-device loopback
/// self-test feeds the decoder the samples the I2S engine actually emitted.

#include "platforms/esp/8266/i2s_ws2812_codec.h"
#include "fl/channels/rx.h"
#include "fl/channels/rx/decode_ws2812.h"
#include "fl/stl/vector.h"
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

// ============================================================================
// TX -> RX loopback: what the #4943 I2S engine emits, reconstructed by the RX
// path (`samplesToEdges`, used by Esp8266I2sRxDevice) and decoded with the
// shared `decodeWs2812Edges` -- a full frame round-trips without hardware.
// ============================================================================

namespace {

/// 4-phase thresholds matching what the codec emits: T0H = 1 slot (312 ns),
/// T1H = 2 slots (625 ns), T0L = 3 slots (937 ns), T1L = 2 slots (625 ns).
ChipsetTiming4Phase loopbackTiming() {
    const ChipsetTiming i2s_ws2812{312, 313, 625, 50, "I2S_WS2812"};
    return make4PhaseTiming(i2s_ws2812, 150);
}

/// Encode `data` as the I2S engine would, reconstruct edges, and decode.
fl::vector<u8> roundTrip(const fl::vector<u8>& data) {
    fl::vector<u16> samples(data.size() * kI2sSamplesPerByte);
    for (size_t i = 0; i < data.size(); ++i) {
        encodeByte(data[i], &samples[i * kI2sSamplesPerByte]);
    }
    fl::vector<EdgeTime> edges;
    samplesToEdges(samples.data(), samples.size(), edges);
    fl::vector<u8> out(data.size() + 1);
    auto r = fl::channels::rx::decodeWs2812Edges(
        loopbackTiming(), fl::span<const EdgeTime>(edges),
        fl::span<u8>(out.data(), out.size()));  // ok span from pointer
    if (!r.ok()) {
        return {};
    }
    out.resize(r.value());
    return out;
}

fl::vector<u8> loopbackPattern(size_t n) {
    fl::vector<u8> v;
    for (size_t i = 0; i < n; ++i) {
        v.push_back(static_cast<u8>(i * 53u + 0x1Bu));
    }
    return v;
}

}  // namespace

FL_TEST_CASE("I2S TX->RX loopback - boundary bytes round-trip") {
    const fl::vector<u8> data = {0x00, 0xFF, 0xAA, 0x55, 0x0F, 0xF0, 0x01, 0x80};
    const fl::vector<u8> got = roundTrip(data);
    FL_REQUIRE(got.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        FL_CHECK(got[i] == data[i]);
    }
}

FL_TEST_CASE("I2S TX->RX loopback - a multi-LED frame round-trips") {
    const fl::vector<u8> data = loopbackPattern(9);  // 3 LEDs x 3 bytes
    const fl::vector<u8> got = roundTrip(data);
    FL_REQUIRE(got.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        FL_CHECK(got[i] == data[i]);
    }
}

FL_TEST_CASE("I2S WS2812 codec - samplesToEdges run-length encodes runs") {
    // 0xF0: first sample 0xCCCC = four '1' nibbles (1100) -> alternating
    // HIGH(2) / LOW(2); second sample 0x8888 = four '0' nibbles (1000) ->
    // HIGH(1) / LOW(3). Starts HIGH, ends LOW; the first run is 2 slots.
    u16 samples[2];
    encodeByte(0xF0, samples);
    fl::vector<EdgeTime> edges;
    samplesToEdges(samples, 2, edges);
    FL_REQUIRE(edges.size() >= 8);
    FL_CHECK(edges[0].high == 1);
    FL_CHECK(edges[edges.size() - 1].high == 0);
    FL_CHECK_EQ(edges[0].ns, static_cast<u32>(2 * kI2sSlotNs));
}

}  // namespace
