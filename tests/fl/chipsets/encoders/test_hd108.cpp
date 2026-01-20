/// @file test_hd108.cpp
/// @brief Unit tests for HD108 encoder functions
///
/// HD108 Protocol:
/// - Start frame: 8 bytes of 0x00
/// - LED data: [Header:2B][R:16b][G:16b][B:16b] (8 bytes per LED)
/// - End frame: (num_leds / 2) + 4 bytes of 0xFF
///
/// Header encoding (dual-byte brightness):
/// - Byte 0: 0x80 | ((brightness_5bit & 0x1F) << 2)
/// - Byte 1: ((brightness_5bit & 0x07) << 5) | (brightness_5bit & 0x1F)
///
/// Tests cover:
/// - encodeHD108() - Global brightness variant
/// - encodeHD108_HD() - Per-LED brightness variant

#include "doctest.h"
#include "fl/chipsets/encoders/hd108.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "fl/ease.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"

namespace test_hd108 {

using namespace fl;

/// Helper to extract 16-bit big-endian value from byte vector
static u16 getBigEndian16(const fl::vector<u8>& data, size_t offset) {
    return (u16(data[offset]) << 8) | u16(data[offset + 1]);
}

/// Helper to verify start frame (8 bytes of 0x00)
static void verifyStartFrame(const fl::vector<u8>& data) {
    REQUIRE(data.size() >= 8);
    for (int i = 0; i < 8; i++) {
        CHECK_EQ(data[i], 0x00);
    }
}

/// Helper to verify end frame (all bytes should be 0xFF)
static void verifyEndFrame(const fl::vector<u8>& data, size_t expected_size) {
    size_t start_offset = data.size() - expected_size;
    for (size_t i = start_offset; i < data.size(); i++) {
        CHECK_EQ(data[i], 0xFF);
    }
}

/// Helper to verify header bytes use maximum gain (31) for all channels
/// Brightness parameter is ignored - gain is always max for maximum precision
static void verifyHeaderBytes(const fl::vector<u8>& data, size_t offset, u8 expected_bri5) {
    (void)expected_bri5;  // Unused - all gains are max (31) regardless of brightness input

    u8 f0 = data[offset];
    u8 f1 = data[offset + 1];

    // Per-channel encoding: f0=[1][RRRRR][GG], f1=[GGG][BBBBB]
    // All channels use maximum gain: R=G=B=31
    u8 expected_f0 = 0xFF;  // [1][11111][11]
    u8 expected_f1 = 0xFF;  // [111][11111]

    CHECK_EQ(f0, expected_f0);
    CHECK_EQ(f1, expected_f1);
}

/// Helper to verify LED RGB16 values with gamma correction
static void verifyLEDData(const fl::vector<u8>& data, size_t offset, u8 r8, u8 g8, u8 b8) {
    // HD108 uses RGB order: R, G, B
    u16 expected_r16 = gamma_2_8(r8);
    u16 expected_g16 = gamma_2_8(g8);
    u16 expected_b16 = gamma_2_8(b8);

    CHECK_EQ(getBigEndian16(data, offset), expected_r16);
    CHECK_EQ(getBigEndian16(data, offset + 2), expected_g16);
    CHECK_EQ(getBigEndian16(data, offset + 4), expected_b16);
}

//-----------------------------------------------------------------------------
// encodeHD108() - Global brightness tests
//-----------------------------------------------------------------------------

TEST_CASE("encodeHD108() - empty range") {
    fl::vector<fl::array<u8, 3>> leds;
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 255);

    // Start frame (8) + End frame (0/2 + 4 = 4) = 12 bytes
    REQUIRE_EQ(output.size(), 12);

    verifyStartFrame(output);
    verifyEndFrame(output, 4);
}

TEST_CASE("encodeHD108() - single LED, max brightness") {
    fl::vector<fl::array<u8, 3>> leds = {{{255, 128, 64}}};
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 255);

    // Start frame (8) + LED (8) + End frame (1/2 + 4 = 4) = 20 bytes
    REQUIRE_EQ(output.size(), 20);

    verifyStartFrame(output);

    // Verify header: brightness 255 -> 5-bit 31
    verifyHeaderBytes(output, 8, 31);

    // Verify RGB data with gamma correction
    verifyLEDData(output, 10, 255, 128, 64);

    verifyEndFrame(output, 4);
}

TEST_CASE("encodeHD108() - single LED, mid brightness") {
    fl::vector<fl::array<u8, 3>> leds = {{{200, 100, 50}}};
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 128);

    REQUIRE_EQ(output.size(), 20);

    // Verify header: brightness 128 -> 5-bit 16
    verifyHeaderBytes(output, 8, 16);

    verifyLEDData(output, 10, 200, 100, 50);
}

TEST_CASE("encodeHD108() - single LED, min brightness") {
    fl::vector<fl::array<u8, 3>> leds = {{{100, 50, 25}}};
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 1);

    REQUIRE_EQ(output.size(), 20);

    // Verify header: brightness 1 -> 5-bit 1 (non-zero preservation)
    verifyHeaderBytes(output, 8, 1);

    verifyLEDData(output, 10, 100, 50, 25);
}

TEST_CASE("encodeHD108() - two LEDs, end frame boundary") {
    fl::vector<fl::array<u8, 3>> leds = {{{255, 0, 0}}, {{0, 255, 0}}};
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 255);

    // Start frame (8) + LEDs (16) + End frame (2/2 + 4 = 5) = 29 bytes
    REQUIRE_EQ(output.size(), 29);

    verifyStartFrame(output);

    // LED 1
    verifyHeaderBytes(output, 8, 31);
    verifyLEDData(output, 10, 255, 0, 0);

    // LED 2
    verifyHeaderBytes(output, 16, 31);
    verifyLEDData(output, 18, 0, 255, 0);

    verifyEndFrame(output, 5);
}

TEST_CASE("encodeHD108() - three LEDs, end frame size") {
    fl::vector<fl::array<u8, 3>> leds = {
        {{255, 0, 0}},
        {{0, 255, 0}},
        {{0, 0, 255}}
    };
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 200);

    // Start frame (8) + LEDs (24) + End frame (3/2 + 4 = 5) = 37 bytes
    REQUIRE_EQ(output.size(), 37);

    verifyStartFrame(output);

    // Verify brightness 200 -> 5-bit 24 (200*31+127)/255 = 24.8
    verifyHeaderBytes(output, 8, 24);

    verifyEndFrame(output, 5);
}

TEST_CASE("encodeHD108() - four LEDs, end frame boundary") {
    fl::vector<fl::array<u8, 3>> leds(4, {{128, 64, 32}});
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 100);

    // Start frame (8) + LEDs (32) + End frame (4/2 + 4 = 6) = 46 bytes
    REQUIRE_EQ(output.size(), 46);

    verifyEndFrame(output, 6);
}

TEST_CASE("encodeHD108() - eight LEDs, end frame size") {
    fl::vector<fl::array<u8, 3>> leds(8, {{200, 150, 100}});
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 150);

    // Start frame (8) + LEDs (64) + End frame (8/2 + 4 = 8) = 80 bytes
    REQUIRE_EQ(output.size(), 80);

    verifyEndFrame(output, 8);
}

TEST_CASE("encodeHD108() - brightness mapping 8-bit to 5-bit") {
    // Test critical brightness values
    struct BrightnessTest {
        u8 input;
        u8 expected_5bit;
    };

    BrightnessTest tests[] = {
        {0, 0},      // Zero maps to zero
        {1, 1},      // Min non-zero preserved
        {8, 1},      // Low values map to 1
        {16, 2},
        {64, 8},
        {127, 15},
        {128, 16},
        {191, 23},
        {192, 23},   // 192*31+127 = 6079/255 = 23.8
        {200, 24},   // 200*31+127 = 6327/255 = 24.8
        {254, 31},
        {255, 31}    // Max maps to max
    };

    for (const auto& test : tests) {
        fl::vector<fl::array<u8, 3>> leds = {{{50, 50, 50}}};
        fl::vector<u8> output;

        encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), test.input);

        verifyHeaderBytes(output, 8, test.expected_5bit);
    }
}

TEST_CASE("encodeHD108() - gamma correction verification") {
    // Verify gamma 2.8 correction is applied to all channels
    fl::vector<fl::array<u8, 3>> leds = {{{255, 128, 64}}};
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 255);

    // Check gamma-corrected 16-bit values
    u16 r16 = getBigEndian16(output, 10);
    u16 g16 = getBigEndian16(output, 12);
    u16 b16 = getBigEndian16(output, 14);

    // Verify they match gamma_2_8 output
    CHECK_EQ(r16, gamma_2_8(255));
    CHECK_EQ(g16, gamma_2_8(128));
    CHECK_EQ(b16, gamma_2_8(64));

    // Gamma 2.8 should produce non-linear values
    CHECK_GT(r16, g16 * 2);  // 255 gamma'd should be > 2x 128 gamma'd
}

TEST_CASE("encodeHD108() - RGB color order") {
    // Verify RGB wire order (not BGR or GRB)
    fl::vector<fl::array<u8, 3>> leds = {{{200, 100, 50}}};
    fl::vector<u8> output;

    encodeHD108(leds.begin(), leds.end(), fl::back_inserter(output), 255);

    // RGB order: R first (offset 10), G second (12), B third (14)
    CHECK_EQ(getBigEndian16(output, 10), gamma_2_8(200));  // Red
    CHECK_EQ(getBigEndian16(output, 12), gamma_2_8(100));  // Green
    CHECK_EQ(getBigEndian16(output, 14), gamma_2_8(50));   // Blue
}

//-----------------------------------------------------------------------------
// encodeHD108_HD() - Per-LED brightness tests
//-----------------------------------------------------------------------------

TEST_CASE("encodeHD108_HD() - empty range") {
    fl::vector<fl::array<u8, 3>> leds;
    fl::vector<u8> brightness;
    fl::vector<u8> output;

    encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));

    // Start frame (8) + End frame (4) = 12 bytes
    REQUIRE_EQ(output.size(), 12);

    verifyStartFrame(output);
    verifyEndFrame(output, 4);
}

TEST_CASE("encodeHD108_HD() - single LED with per-LED brightness") {
    fl::vector<fl::array<u8, 3>> leds = {{{255, 128, 64}}};
    fl::vector<u8> brightness = {200};
    fl::vector<u8> output;

    encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));

    // Start frame (8) + LED (8) + End frame (4) = 20 bytes
    REQUIRE_EQ(output.size(), 20);

    verifyStartFrame(output);

    // Verify per-LED brightness 200 -> 5-bit 24
    verifyHeaderBytes(output, 8, 24);

    verifyLEDData(output, 10, 255, 128, 64);

    verifyEndFrame(output, 4);
}

TEST_CASE("encodeHD108_HD() - multiple LEDs with varying brightness") {
    fl::vector<fl::array<u8, 3>> leds = {
        {{255, 0, 0}},
        {{0, 255, 0}},
        {{0, 0, 255}}
    };
    fl::vector<u8> brightness = {255, 128, 64};
    fl::vector<u8> output;

    encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));

    // Start frame (8) + LEDs (24) + End frame (5) = 37 bytes
    REQUIRE_EQ(output.size(), 37);

    verifyStartFrame(output);

    // LED 1: brightness 255 -> 31
    verifyHeaderBytes(output, 8, 31);
    verifyLEDData(output, 10, 255, 0, 0);

    // LED 2: brightness 128 -> 16
    verifyHeaderBytes(output, 16, 16);
    verifyLEDData(output, 18, 0, 255, 0);

    // LED 3: brightness 64 -> 8
    verifyHeaderBytes(output, 24, 8);
    verifyLEDData(output, 26, 0, 0, 255);

    verifyEndFrame(output, 5);
}

TEST_CASE("encodeHD108_HD() - brightness caching optimization") {
    // When consecutive LEDs have same brightness, header should be cached
    fl::vector<fl::array<u8, 3>> leds = {
        {{100, 0, 0}},
        {{0, 100, 0}},
        {{0, 0, 100}}
    };
    fl::vector<u8> brightness = {200, 200, 200};  // Same brightness
    fl::vector<u8> output;

    encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 37);

    // All LEDs should have same header bytes (brightness 200 -> 24)
    verifyHeaderBytes(output, 8, 24);
    verifyHeaderBytes(output, 16, 24);
    verifyHeaderBytes(output, 24, 24);

    // Verify colors differ despite same brightness
    verifyLEDData(output, 10, 100, 0, 0);
    verifyLEDData(output, 18, 0, 100, 0);
    verifyLEDData(output, 26, 0, 0, 100);
}

TEST_CASE("encodeHD108_HD() - end frame calculation") {
    // Test end frame size: (num_leds / 2) + 4

    // 1 LED: 1/2 + 4 = 4 bytes
    {
        fl::vector<fl::array<u8, 3>> leds(1, {{50, 50, 50}});
        fl::vector<u8> brightness(1, 100);
        fl::vector<u8> output;

        encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));
        REQUIRE_EQ(output.size(), 8 + 8 + 4);
        verifyEndFrame(output, 4);
    }

    // 2 LEDs: 2/2 + 4 = 5 bytes
    {
        fl::vector<fl::array<u8, 3>> leds(2, {{50, 50, 50}});
        fl::vector<u8> brightness(2, 100);
        fl::vector<u8> output;

        encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));
        REQUIRE_EQ(output.size(), 8 + 16 + 5);
        verifyEndFrame(output, 5);
    }

    // 10 LEDs: 10/2 + 4 = 9 bytes
    {
        fl::vector<fl::array<u8, 3>> leds(10, {{50, 50, 50}});
        fl::vector<u8> brightness(10, 100);
        fl::vector<u8> output;

        encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));
        REQUIRE_EQ(output.size(), 8 + 80 + 9);
        verifyEndFrame(output, 9);
    }
}

TEST_CASE("encodeHD108_HD() - min/max brightness values") {
    fl::vector<fl::array<u8, 3>> leds = {{{100, 100, 100}}, {{150, 150, 150}}};
    fl::vector<u8> brightness = {0, 255};
    fl::vector<u8> output;

    encodeHD108_HD(leds.begin(), leds.end(), brightness.begin(), fl::back_inserter(output));

    // LED 1: brightness 0 -> 0
    verifyHeaderBytes(output, 8, 0);

    // LED 2: brightness 255 -> 31
    verifyHeaderBytes(output, 16, 31);
}

//-----------------------------------------------------------------------------
// Helper function tests
//-----------------------------------------------------------------------------

TEST_CASE("hd108BrightnessHeader() - max gain encoding") {
    // Test the per-channel gain header generation function
    // All channels use maximum gain (31) regardless of brightness input
    // Brightness control is handled via 16-bit PWM values, not gain

    struct HeaderTest {
        u8 brightness_8bit;  // Input brightness (now ignored)
        u8 expected_gain;    // Expected gain for all channels
        u8 expected_f0;      // Expected header byte 0
        u8 expected_f1;      // Expected header byte 1
    };

    // All brightness inputs produce the same output: R=G=B=31 (max gain)
    // f0: [1][11111][11] = 0xFF, f1: [111][11111] = 0xFF
    HeaderTest tests[] = {
        {0,   31, 0xFF, 0xFF},  // brightness=0   → gain=31 (max)
        {1,   31, 0xFF, 0xFF},  // brightness=1   → gain=31 (max)
        {64,  31, 0xFF, 0xFF},  // brightness=64  → gain=31 (max)
        {128, 31, 0xFF, 0xFF},  // brightness=128 → gain=31 (max)
        {200, 31, 0xFF, 0xFF},  // brightness=200 → gain=31 (max)
        {255, 31, 0xFF, 0xFF}   // brightness=255 → gain=31 (max)
    };

    for (const auto& test : tests) {
        u8 f0, f1;
        hd108BrightnessHeader(test.brightness_8bit, &f0, &f1);

        CHECK_EQ(f0, test.expected_f0);
        CHECK_EQ(f1, test.expected_f1);

        // Verify all channels use maximum gain (31)
        u8 extracted_r = (f0 >> 2) & 0x1F;            // Extract R gain (bits 6-2 of f0)
        u8 extracted_g_hi = f0 & 0x03;                // Extract G gain high bits (bits 1-0 of f0)
        u8 extracted_g_lo = (f1 >> 5) & 0x07;         // Extract G gain low bits (bits 7-5 of f1)
        u8 extracted_g = (extracted_g_hi << 3) | extracted_g_lo;  // Reconstruct G gain
        u8 extracted_b = f1 & 0x1F;                   // Extract B gain (bits 4-0 of f1)

        CHECK_EQ(extracted_r, test.expected_gain);
        CHECK_EQ(extracted_g, test.expected_gain);
        CHECK_EQ(extracted_b, test.expected_gain);
    }
}

TEST_CASE("hd108GammaCorrect() - gamma 2.8 correction") {
    // Test gamma correction function directly

    CHECK_EQ(hd108GammaCorrect(0), gamma_2_8(0));
    CHECK_EQ(hd108GammaCorrect(64), gamma_2_8(64));
    CHECK_EQ(hd108GammaCorrect(128), gamma_2_8(128));
    CHECK_EQ(hd108GammaCorrect(192), gamma_2_8(192));
    CHECK_EQ(hd108GammaCorrect(255), gamma_2_8(255));

    // Verify non-linearity (gamma > 1.0 means output grows faster than input)
    u16 v64 = hd108GammaCorrect(64);
    u16 v128 = hd108GammaCorrect(128);
    u16 v255 = hd108GammaCorrect(255);

    // Gamma 2.8: 128 should be < 255/2 (non-linear curve)
    CHECK_LT(v128, v255 / 2);

    // 64 should be much less than 128/2
    CHECK_LT(v64, v128 / 2);
}

} // namespace test_hd108
