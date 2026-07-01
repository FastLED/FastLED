/// @file wave8_encoder_i2s1.cpp
/// @brief Host tests for `wave8_encoder_i2s1` (FastLED#3526 Phase 2a).
///
/// Byte-exact verification that the I2S1 encoder:
///   1. Reuses the shared `fl::wave8Transpose_*` kernels (bit-identical
///      output to a direct-call baseline).
///   2. Handles lane-strided input correctly.
///   3. Zero-pads inactive lanes without polluting the transpose.
///   4. Rejects out-of-range inputs cleanly.

#ifdef FASTLED_STUB_IMPL

#include "test.h"
#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s1.h"
#include "fl/channels/wave8.h"
#include "fl/channels/detail/wave8.hpp"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Build a canonical WS2812B expansion LUT once and share across tests.
static Wave8ByteExpansionLut buildWs2812Lut() FL_NO_EXCEPT {
    ChipsetTiming timing = to_runtime_timing<TIMING_WS2812_800KHZ>();
    Wave8BitExpansionLut nibble = buildWave8ExpansionLUT(timing);
    return buildWave8ByteExpansionLUT(nibble);
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — rejects zero lanes") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();
    fl::vector<fl::u8> input(16 * 3, 0);
    fl::vector<fl::u8> output(wave8I2s1EncodedFrameSize(3), 0);
    bool ok = encodeChannelWave8_i2s1(fl::span<const fl::u8>(input),
                                       /*bytes_per_lane=*/3,
                                       /*num_lanes=*/0,
                                       lut,
                                       fl::span<fl::u8>(output));
    FL_REQUIRE(!ok);
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — rejects too many lanes") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();
    fl::vector<fl::u8> input(16 * 3, 0);
    fl::vector<fl::u8> output(wave8I2s1EncodedFrameSize(3), 0);
    bool ok = encodeChannelWave8_i2s1(fl::span<const fl::u8>(input),
                                       3,
                                       /*num_lanes=*/17,
                                       lut,
                                       fl::span<fl::u8>(output));
    FL_REQUIRE(!ok);
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — rejects undersized input") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();
    // Should need 16*3=48 bytes; give it 47.
    fl::vector<fl::u8> input(47, 0);
    fl::vector<fl::u8> output(wave8I2s1EncodedFrameSize(3), 0);
    bool ok = encodeChannelWave8_i2s1(fl::span<const fl::u8>(input),
                                       3, 4, lut,
                                       fl::span<fl::u8>(output));
    FL_REQUIRE(!ok);
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — rejects undersized output") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();
    fl::vector<fl::u8> input(16 * 3, 0);
    // Need 3 * 128 = 384 bytes; give it 100.
    fl::vector<fl::u8> output(100, 0);
    bool ok = encodeChannelWave8_i2s1(fl::span<const fl::u8>(input),
                                       3, 4, lut,
                                       fl::span<fl::u8>(output));
    FL_REQUIRE(!ok);
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — one-lane one-byte matches direct call to wave8Transpose_16_bf1") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();

    // Input: lane 0 has one byte (0xA5); lanes 1-15 zero-padded.
    fl::vector<fl::u8> input(16, 0);
    input[0] = 0xA5;

    // Encoder output.
    constexpr size_t kBytesPerLane = 1;
    fl::vector<fl::u8> encoded(wave8I2s1EncodedFrameSize(kBytesPerLane), 0);
    bool ok = encodeChannelWave8_i2s1(
        fl::span<const fl::u8>(input),
        kBytesPerLane,
        /*num_lanes=*/1,
        lut,
        fl::span<fl::u8>(encoded));
    FL_REQUIRE(ok);

    // Reference: direct call to wave8Transpose_16_bf1 with the same input.
    // If the encoder duplicates or re-implements the transpose, this
    // will diverge.
    fl::u8 lanes_ref[16] = {0};
    lanes_ref[0] = 0xA5;
    fl::u8 output_ref[16 * sizeof(Wave8Byte)] = {0};
    wave8Transpose_16_bf1(reinterpret_cast<const fl::u8(&)[16]>(lanes_ref), lut,
                           output_ref);

    for (size_t i = 0; i < sizeof(output_ref); ++i) {
        FL_REQUIRE_EQ(encoded[i], output_ref[i]);
    }
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — four-byte batch matches wave8Transpose_16x4_bf1_pipe4") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();

    // 4 bytes per lane × 16 lanes = 64 input bytes.
    // Fill lane 0 with a rising ramp, others with a distinct byte per lane.
    constexpr size_t kBytesPerLane = 4;
    fl::vector<fl::u8> input(16 * kBytesPerLane, 0);
    for (size_t lane = 0; lane < 4; ++lane) {
        for (size_t b = 0; b < kBytesPerLane; ++b) {
            input[lane * kBytesPerLane + b] = static_cast<fl::u8>(0x10 * (lane + 1) + b);
        }
    }

    fl::vector<fl::u8> encoded(wave8I2s1EncodedFrameSize(kBytesPerLane), 0);
    bool ok = encodeChannelWave8_i2s1(
        fl::span<const fl::u8>(input),
        kBytesPerLane,
        /*num_lanes=*/4,
        lut,
        fl::span<fl::u8>(encoded));
    FL_REQUIRE(ok);

    // Reference: direct pipe4 call on the same four columns.
    fl::u8 la[16] = {0}, lb[16] = {0}, lc[16] = {0}, ld[16] = {0};
    for (size_t lane = 0; lane < 4; ++lane) {
        la[lane] = input[lane * kBytesPerLane + 0];
        lb[lane] = input[lane * kBytesPerLane + 1];
        lc[lane] = input[lane * kBytesPerLane + 2];
        ld[lane] = input[lane * kBytesPerLane + 3];
    }
    constexpr size_t kBlockSize = 16 * sizeof(Wave8Byte);
    fl::u8 ref[4 * kBlockSize] = {0};
    wave8Transpose_16x4_bf1_pipe4(
        reinterpret_cast<const fl::u8(&)[16]>(la),
        reinterpret_cast<const fl::u8(&)[16]>(lb),
        reinterpret_cast<const fl::u8(&)[16]>(lc),
        reinterpret_cast<const fl::u8(&)[16]>(ld),
        lut,
        *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref),
        *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref + kBlockSize),
        *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref + 2 * kBlockSize),
        *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref + 3 * kBlockSize));

    for (size_t i = 0; i < sizeof(ref); ++i) {
        FL_REQUIRE_EQ(encoded[i], ref[i]);
    }
}

FL_TEST_CASE("encodeChannelWave8_i2s1 — inactive-lane bytes ignored") {
    Wave8ByteExpansionLut lut = buildWs2812Lut();

    constexpr size_t kBytesPerLane = 2;
    // Lane 0 gets real data. Lanes 1..15 get garbage that MUST be ignored
    // because num_lanes=1. If garbage leaked into the transpose, the
    // output would differ from the zero-padded reference.
    fl::vector<fl::u8> input(16 * kBytesPerLane, 0);
    input[0 * kBytesPerLane + 0] = 0x33;
    input[0 * kBytesPerLane + 1] = 0xCC;
    for (size_t lane = 1; lane < 16; ++lane) {
        input[lane * kBytesPerLane + 0] = 0xAA;
        input[lane * kBytesPerLane + 1] = 0x55;
    }

    fl::vector<fl::u8> encoded(wave8I2s1EncodedFrameSize(kBytesPerLane), 0);
    bool ok = encodeChannelWave8_i2s1(
        fl::span<const fl::u8>(input),
        kBytesPerLane,
        /*num_lanes=*/1,
        lut,
        fl::span<fl::u8>(encoded));
    FL_REQUIRE(ok);

    // Reference: zero-padded lanes 1..15.
    fl::u8 lanes_a[16] = {0};
    lanes_a[0] = 0x33;
    fl::u8 lanes_b[16] = {0};
    lanes_b[0] = 0xCC;
    constexpr size_t kBlockSize = 16 * sizeof(Wave8Byte);
    fl::u8 ref[2 * kBlockSize] = {0};
    wave8Transpose_16_bf1(reinterpret_cast<const fl::u8(&)[16]>(lanes_a), lut,
                           *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref));
    wave8Transpose_16_bf1(reinterpret_cast<const fl::u8(&)[16]>(lanes_b), lut,
                           *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref + kBlockSize));

    for (size_t i = 0; i < sizeof(ref); ++i) {
        FL_REQUIRE_EQ(encoded[i], ref[i]);
    }
}

// ============================================================================
// Wave3 sibling tests (FastLED#3526 Phase 2a follow-up)
// ============================================================================

FL_TEST_CASE("encodeChannelWave3_i2s1 — WS2812 is wave3-eligible and encodes byte-exact") {
    // WS2812 canonical timing: T1=250, T2=625, T3=375 → period 1250 ns.
    // canUseWave3 must accept this so wave3 encoding is available.
    ChipsetTiming timing = to_runtime_timing<TIMING_WS2812_800KHZ>();
    FL_REQUIRE(canUseWave3(timing));

    Wave3BitExpansionLut lut = buildWave3ExpansionLUT(timing);

    // 1 lane, 3 bytes/lane. Total input frame is 16*3 = 48 bytes.
    // Encoded output is bytes_per_lane * (16 * sizeof(Wave3Byte))
    //                 = 3 * 48 = 144 bytes.
    fl::vector<fl::u8> input(16 * 3, 0);
    input[0] = 0x33;
    input[1] = 0xCC;
    input[2] = 0x55;
    fl::vector<fl::u8> encoded(wave3I2s1EncodedFrameSize(3), 0xAB);

    bool ok = encodeChannelWave3_i2s1(fl::span<const fl::u8>(input),
                                       /*bytes_per_lane=*/3,
                                       /*num_lanes=*/1, lut,
                                       fl::span<fl::u8>(encoded));
    FL_REQUIRE(ok);

    // Reference: run the shared wave3Transpose_16 kernel directly on the
    // per-byte-position lane arrays and compare byte-exact. Same anti-drift
    // pattern as the wave8 test above.
    constexpr size_t kBlockSize = 16 * sizeof(Wave3Byte);
    fl::u8 ref[3 * kBlockSize] = {0};
    for (size_t byte_pos = 0; byte_pos < 3; ++byte_pos) {
        fl::u8 lanes[16] = {0};
        lanes[0] = input[byte_pos];
        wave3Transpose_16(
            reinterpret_cast<const fl::u8(&)[16]>(lanes), lut,
            *reinterpret_cast<fl::u8(*)[kBlockSize]>(ref + byte_pos * kBlockSize));
    }

    for (size_t i = 0; i < sizeof(ref); ++i) {
        FL_REQUIRE_EQ(encoded[i], ref[i]);
    }
}

FL_TEST_CASE("encodeChannelWave3_i2s1 — rejects out-of-range inputs") {
    ChipsetTiming timing = to_runtime_timing<TIMING_WS2812_800KHZ>();
    Wave3BitExpansionLut lut = buildWave3ExpansionLUT(timing);

    fl::vector<fl::u8> input(16 * 3, 0);
    fl::vector<fl::u8> output(wave3I2s1EncodedFrameSize(3), 0);

    // Zero lanes rejected.
    FL_REQUIRE(!encodeChannelWave3_i2s1(fl::span<const fl::u8>(input), 3, 0, lut,
                                         fl::span<fl::u8>(output)));
    // 17 lanes rejected.
    FL_REQUIRE(!encodeChannelWave3_i2s1(fl::span<const fl::u8>(input), 3, 17, lut,
                                         fl::span<fl::u8>(output)));
    // Undersized input rejected.
    fl::vector<fl::u8> short_input(47, 0);
    FL_REQUIRE(!encodeChannelWave3_i2s1(fl::span<const fl::u8>(short_input), 3, 1,
                                         lut, fl::span<fl::u8>(output)));
    // Undersized output rejected.
    fl::vector<fl::u8> short_output(wave3I2s1EncodedFrameSize(3) - 1, 0);
    FL_REQUIRE(!encodeChannelWave3_i2s1(fl::span<const fl::u8>(input), 3, 1, lut,
                                         fl::span<fl::u8>(short_output)));
}

}  // FL_TEST_FILE

#endif  // FASTLED_STUB_IMPL
