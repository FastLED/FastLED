/// @file test_wave8_spi.cpp
/// @brief Unit tests for SPI wave8 encoder
///
/// Tests wave8 encoding for ESP32 SPI channel engine, including:
/// - Single-lane encoding (no transposition)
/// - Dual-lane encoding (2-lane transposition)
/// - Quad-lane encoding (4-lane transposition)
///
/// Note: This test file does NOT test convertSpiTimingToChipsetTiming() because
/// that function requires ESP32-specific headers that aren't available in the
/// stub platform. That conversion will be tested in integration tests on real hardware.

#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"

// Include the full wave8_encoder_spi.h header
#include "platforms/esp/32/drivers/spi/wave8_encoder_spi.h"
#include "fl/stl/cstddef.h"  // for size_t
#include "fl/stl/stdint.h"  // for uint8_t
#include "doctest.h"  // for ResultBuilder, CHECK, etc.
#include "fl/slice.h"  // for span

using namespace fl;

// ============================================================================
// Helper: Create ChipsetTiming for WS2812 (standard test case)
// ============================================================================
ChipsetTiming createWS2812ChipsetTiming() {
    ChipsetTiming timing;
    timing.T1 = 800;  // bit1 HIGH time: 800nsyhj
    timing.T2 = 400;  // bit1 LOW time: 400ns
    timing.T3 = 400;  // bit0 HIGH time: 400ns
    return timing;
}

// Note: convertSpiTimingToChipsetTiming() tests are skipped in stub platform tests
// because they require ESP32-specific headers. These will be tested in integration
// tests on real hardware.

// ============================================================================
// Test: Single-Lane Encoding (No Transposition)
// ============================================================================

TEST_CASE("wave8EncodeSingleLane_basic") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: Single byte (0xFF = all bits '1')
    uint8_t input[] = {0xFF};
    uint8_t output[8];  // 1 byte → 8 bytes (1 Wave8Byte)

    size_t written = wave8EncodeSingleLane(
        fl::span<const uint8_t>(input, 1),
        fl::span<uint8_t>(output, 8),
        lut);

    REQUIRE(written == 8);

    // For 0xFF (all bits '1'), all Wave8Bit symbols should use bit1 waveform
    // WS2812 bit1: 2/3 HIGH, 1/3 LOW → wave8 with 8 pulses: ~5-6 HIGH pulses
    // Expected pattern: approximately 0b11111100 to 0b11111110 per Wave8Bit
    // (exact value depends on rounding in buildWave8ExpansionLUT)

    // Verify all 8 Wave8Bit symbols are non-zero (at least some HIGH pulses)
    for (int i = 0; i < 8; i++) {
        REQUIRE(output[i] != 0x00);
    }
}

TEST_CASE("wave8EncodeSingleLane_zero_byte") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: Single byte (0x00 = all bits '0')
    uint8_t input[] = {0x00};
    uint8_t output[8];  // 1 byte → 8 bytes

    size_t written = wave8EncodeSingleLane(
        fl::span<const uint8_t>(input, 1),
        fl::span<uint8_t>(output, 8),
        lut);

    REQUIRE(written == 8);

    // For 0x00 (all bits '0'), all Wave8Bit symbols should use bit0 waveform
    // WS2812 bit0: 1/3 HIGH, 2/3 LOW → wave8 with 8 pulses: ~2-3 HIGH pulses
    // Expected pattern: approximately 0b11000000 to 0b11100000 per Wave8Bit

    // Verify all 8 Wave8Bit symbols have SOME HIGH pulses (not all zeros)
    // but not all HIGH (to distinguish from bit1)
    for (int i = 0; i < 8; i++) {
        REQUIRE(output[i] != 0x00);  // Not all zeros
        REQUIRE(output[i] != 0xFF);  // Not all ones
    }
}

TEST_CASE("wave8EncodeSingleLane_multiple_bytes") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: Multiple bytes (RGB values)
    uint8_t input[] = {0xFF, 0x00, 0xAA};  // Red=max, Green=0, Blue=alternating
    uint8_t output[24];  // 3 bytes → 24 bytes (3 Wave8Byte)

    size_t written = wave8EncodeSingleLane(
        fl::span<const uint8_t>(input, 3),
        fl::span<uint8_t>(output, 24),
        lut);

    REQUIRE(written == 24);

    // Verify output is non-zero (encoding produced data)
    bool has_data = false;
    for (int i = 0; i < 24; i++) {
        if (output[i] != 0x00) {
            has_data = true;
            break;
        }
    }
    REQUIRE(has_data);
}

TEST_CASE("wave8EncodeSingleLane_buffer_too_small") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: 1 byte
    uint8_t input[] = {0xFF};
    uint8_t output[4];  // Only 4 bytes (need 8)

    size_t written = wave8EncodeSingleLane(
        fl::span<const uint8_t>(input, 1),
        fl::span<uint8_t>(output, 4),
        lut);

    // Should detect buffer too small and return 0
    REQUIRE(written == 0);
}

// ============================================================================
// Test: Dual-Lane Encoding (2-Lane Transposition)
// ============================================================================

TEST_CASE("wave8EncodeDualLane_basic") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: 1 byte per lane
    uint8_t lane0[] = {0xFF};  // All bits '1'
    uint8_t lane1[] = {0x00};  // All bits '0'
    uint8_t output[16];        // 2 lanes × 8 bytes = 16 bytes

    size_t written = wave8EncodeDualLane(
        fl::span<const uint8_t>(lane0, 1),
        fl::span<const uint8_t>(lane1, 1),
        fl::span<uint8_t>(output, 16),
        lut);

    REQUIRE(written == 16);

    // Verify output has data (non-zero bytes from lane0)
    bool has_data = false;
    for (int i = 0; i < 16; i++) {
        if (output[i] != 0x00) {
            has_data = true;
            break;
        }
    }
    REQUIRE(has_data);
}

TEST_CASE("wave8EncodeDualLane_lane_size_mismatch") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: Different lane sizes (should fail)
    uint8_t lane0[] = {0xFF, 0xAA};  // 2 bytes
    uint8_t lane1[] = {0x00};        // 1 byte
    uint8_t output[32];

    size_t written = wave8EncodeDualLane(
        fl::span<const uint8_t>(lane0, 2),
        fl::span<const uint8_t>(lane1, 1),
        fl::span<uint8_t>(output, 32),
        lut);

    // Should detect lane size mismatch and return 0
    REQUIRE(written == 0);
}

TEST_CASE("wave8EncodeDualLane_buffer_too_small") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: 1 byte per lane
    uint8_t lane0[] = {0xFF};
    uint8_t lane1[] = {0x00};
    uint8_t output[8];  // Only 8 bytes (need 16)

    size_t written = wave8EncodeDualLane(
        fl::span<const uint8_t>(lane0, 1),
        fl::span<const uint8_t>(lane1, 1),
        fl::span<uint8_t>(output, 8),
        lut);

    // Should detect buffer too small and return 0
    REQUIRE(written == 0);
}

// ============================================================================
// Test: Quad-Lane Encoding (4-Lane Transposition)
// ============================================================================

TEST_CASE("wave8EncodeQuadLane_basic") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: 1 byte per lane
    uint8_t lane0_data[] = {0xFF};
    uint8_t lane1_data[] = {0xAA};
    uint8_t lane2_data[] = {0x55};
    uint8_t lane3_data[] = {0x00};

    fl::span<const uint8_t> lanes[4] = {
        fl::span<const uint8_t>(lane0_data, 1),
        fl::span<const uint8_t>(lane1_data, 1),
        fl::span<const uint8_t>(lane2_data, 1),
        fl::span<const uint8_t>(lane3_data, 1)
    };

    uint8_t output[32];  // 4 lanes × 8 bytes = 32 bytes

    size_t written = wave8EncodeQuadLane(lanes, fl::span<uint8_t>(output, 32), lut);

    REQUIRE(written == 32);

    // Verify output has data (non-zero bytes from active lanes)
    bool has_data = false;
    for (int i = 0; i < 32; i++) {
        if (output[i] != 0x00) {
            has_data = true;
            break;
        }
    }
    REQUIRE(has_data);
}

TEST_CASE("wave8EncodeQuadLane_lane_size_mismatch") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: Different lane sizes (should fail)
    uint8_t lane0_data[] = {0xFF, 0xAA};  // 2 bytes
    uint8_t lane1_data[] = {0xAA};        // 1 byte
    uint8_t lane2_data[] = {0x55};        // 1 byte
    uint8_t lane3_data[] = {0x00};        // 1 byte

    fl::span<const uint8_t> lanes[4] = {
        fl::span<const uint8_t>(lane0_data, 2),
        fl::span<const uint8_t>(lane1_data, 1),
        fl::span<const uint8_t>(lane2_data, 1),
        fl::span<const uint8_t>(lane3_data, 1)
    };

    uint8_t output[64];

    size_t written = wave8EncodeQuadLane(lanes, fl::span<uint8_t>(output, 64), lut);

    // Should detect lane size mismatch and return 0
    REQUIRE(written == 0);
}

TEST_CASE("wave8EncodeQuadLane_buffer_too_small") {
    // Create WS2812 timing and build LUT
    ChipsetTiming chipsetTiming = createWS2812ChipsetTiming();
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);

    // Input: 1 byte per lane
    uint8_t lane0_data[] = {0xFF};
    uint8_t lane1_data[] = {0xAA};
    uint8_t lane2_data[] = {0x55};
    uint8_t lane3_data[] = {0x00};

    fl::span<const uint8_t> lanes[4] = {
        fl::span<const uint8_t>(lane0_data, 1),
        fl::span<const uint8_t>(lane1_data, 1),
        fl::span<const uint8_t>(lane2_data, 1),
        fl::span<const uint8_t>(lane3_data, 1)
    };

    uint8_t output[16];  // Only 16 bytes (need 32)

    size_t written = wave8EncodeQuadLane(lanes, fl::span<uint8_t>(output, 16), lut);

    // Should detect buffer too small and return 0
    REQUIRE(written == 0);
}

// ============================================================================
// Test: Output Buffer Size Calculation
// ============================================================================

TEST_CASE("wave8CalculateOutputSize") {
    // Single-lane: 1 byte → 8 bytes
    REQUIRE(wave8CalculateOutputSize(1, 1) == 8);
    REQUIRE(wave8CalculateOutputSize(100, 1) == 800);

    // Dual-lane: 1 byte → 16 bytes
    REQUIRE(wave8CalculateOutputSize(1, 2) == 16);
    REQUIRE(wave8CalculateOutputSize(100, 2) == 1600);

    // Quad-lane: 1 byte → 32 bytes
    REQUIRE(wave8CalculateOutputSize(1, 4) == 32);
    REQUIRE(wave8CalculateOutputSize(100, 4) == 3200);
}

// ============================================================================
// Test: Comparison with Legacy encodeLedByte() (Future Integration Test)
// ============================================================================
// Note: This test is commented out because it requires access to
// ChannelEngineSpi::encodeLedByte(), which is a private static method.
// When integrating wave8 into channel_engine_spi, this test can be enabled
// to verify backward compatibility.

/*
TEST_CASE("wave8_vs_legacy_encoding") {
    // Create WS2812 timing
    SpiTimingConfig spiTiming = createWS2812Timing();

    // Legacy encoding (bit-by-bit)
    uint8_t legacy_output[32];
    fl::memset(legacy_output, 0, sizeof(legacy_output));
    uint8_t test_byte = 0xAA;  // Alternating bits: 10101010
    uint32_t legacy_bits = ChannelEngineSpi::encodeLedByte(
        test_byte, legacy_output, spiTiming, 0);

    // Wave8 encoding
    ChipsetTiming chipsetTiming = convertSpiTimingToChipsetTiming(spiTiming);
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(chipsetTiming);
    uint8_t wave8_output[8];
    size_t wave8_bytes = wave8EncodeSingleLane(
        fl::span<const uint8_t>(&test_byte, 1),
        fl::span<uint8_t>(wave8_output, 8),
        lut);

    // Compare outputs (should match bit-for-bit)
    size_t legacy_bytes = (legacy_bits + 7) / 8;
    REQUIRE(wave8_bytes == legacy_bytes);
    REQUIRE(fl::memcmp(wave8_output, legacy_output, legacy_bytes) == 0);
}
*/
