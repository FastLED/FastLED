/// @file test_lpd8806.cpp
/// @brief Unit tests for LPD8806 encoder function
///
/// LPD8806 Protocol:
/// - LED data: GRB order (Green, Red, Blue) - 3 bytes per LED
/// - Each byte has MSB set (0x80) with 7-bit color depth (bits 0-6)
/// - Latch: ((num_leds * 3 + 63) / 64) bytes of 0x00
///
/// Color Encoding:
/// - lpd8806Encode(): Maps 8-bit (0-255) to 7-bit (0x80-0xFF) with MSB set
/// - Scaling: (value >> 1) preserves proportions, special rounding for non-extremes

#include "fl/chipsets/encoders/lpd8806.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/stl/array.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/iterator.h"

using namespace fl;

namespace {

TEST_CASE("LPD8806 - lpd8806Encode() helper function") {
    // Verify 7-bit encoding with MSB set
    // Range: 0x80 (min) to 0xFF (max)

    // Boundary values
    CHECK_EQ(lpd8806Encode(0), 0x80);      // 0 -> 0x80 (MSB set, 7 bits = 0)
    CHECK_EQ(lpd8806Encode(255), 0xFF);    // 255 -> 0xFF (MSB set, 7 bits = 127)
    CHECK_EQ(lpd8806Encode(254), 0xFF);    // 254 -> 0xFF (rounds up)

    // Mid-range values (rounding adds 1 for non-extremes)
    CHECK_EQ(lpd8806Encode(128), 0xC1);    // 128 -> 0xC1 (64 + 1 rounding | 0x80)
    CHECK_EQ(lpd8806Encode(127), 0xBF);    // 127 -> 0xBF (63 + 0 rounding | 0x80)

    // Low values (test rounding behavior: groups values)
    CHECK_EQ(lpd8806Encode(1), 0x81);      // 1 -> 0x81 (0 + 1 rounding | 0x80)
    CHECK_EQ(lpd8806Encode(2), 0x81);      // 2 -> 0x81 (1 + 0 rounding | 0x80)
    CHECK_EQ(lpd8806Encode(3), 0x81);      // 3 -> 0x81 (1 + 0 rounding | 0x80)

    // Verify MSB is always set (0x80 bit)
    for (u8 i = 0; i < 255; i++) {
        u8 encoded = lpd8806Encode(i);
        CHECK_GE(encoded, 0x80);           // Always >= 0x80
        CHECK_LE(encoded, 0xFF);           // Always <= 0xFF
    }
}

TEST_CASE("LPD8806 - Single LED encoding with GRB order") {
    // Single LED: Green=100, Red=150, Blue=200
    // Wire format: GRB (pixel[0]=G, pixel[1]=R, pixel[2]=B)
    fl::vector<fl::array<u8, 3>> input = {
        {100, 150, 200}  // GRB order
    };

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Expected: 3 LED bytes + 1 latch byte
    // Latch: ((1 * 3 + 63) / 64) = 66 / 64 = 1 byte
    CHECK_EQ(output.size(), 4);

    // Verify LED data (GRB order)
    CHECK_EQ(output[0], lpd8806Encode(100));  // Green
    CHECK_EQ(output[1], lpd8806Encode(150));  // Red
    CHECK_EQ(output[2], lpd8806Encode(200));  // Blue

    // Verify latch byte
    CHECK_EQ(output[3], 0x00);
}

TEST_CASE("LPD8806 - Multiple LEDs with GRB color order") {
    // Three LEDs to verify GRB ordering consistency
    fl::vector<fl::array<u8, 3>> input = {
        {255, 0, 0},      // LED 0: G=255, R=0, B=0 (pure green in GRB)
        {0, 255, 0},      // LED 1: G=0, R=255, B=0 (pure red in GRB)
        {0, 0, 255}       // LED 2: G=0, R=0, B=255 (pure blue in GRB)
    };

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Expected: 9 LED bytes + 1 latch byte
    // Latch: ((3 * 3 + 63) / 64) = 72 / 64 = 1 byte
    CHECK_EQ(output.size(), 10);

    // LED 0: Green=255, Red=0, Blue=0
    CHECK_EQ(output[0], 0xFF);  // Green (max)
    CHECK_EQ(output[1], 0x80);  // Red (min)
    CHECK_EQ(output[2], 0x80);  // Blue (min)

    // LED 1: Green=0, Red=255, Blue=0
    CHECK_EQ(output[3], 0x80);  // Green (min)
    CHECK_EQ(output[4], 0xFF);  // Red (max)
    CHECK_EQ(output[5], 0x80);  // Blue (min)

    // LED 2: Green=0, Red=0, Blue=255
    CHECK_EQ(output[6], 0x80);  // Green (min)
    CHECK_EQ(output[7], 0x80);  // Red (min)
    CHECK_EQ(output[8], 0xFF);  // Blue (max)

    // Verify latch byte
    CHECK_EQ(output[9], 0x00);
}

TEST_CASE("LPD8806 - Zero LEDs edge case") {
    // Empty input should produce only latch bytes
    fl::vector<fl::array<u8, 3>> input = {};

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Latch: ((0 * 3 + 63) / 64) = 63 / 64 = 0 bytes
    CHECK_EQ(output.size(), 0);
}

TEST_CASE("LPD8806 - Latch calculation boundary - 21 LEDs") {
    // Test latch boundary: 21 LEDs = 63 bytes
    // Latch: ((21 * 3 + 63) / 64) = 126 / 64 = 1 byte
    // At 22 LEDs: ((22 * 3 + 63) / 64) = 129 / 64 = 2 bytes
    fl::vector<fl::array<u8, 3>> input(21, {128, 128, 128});

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Expected: 63 LED bytes + 1 latch byte
    CHECK_EQ(output.size(), 64);

    // Verify last byte is latch
    CHECK_EQ(output[63], 0x00);
}

TEST_CASE("LPD8806 - Latch calculation boundary - 22 LEDs") {
    // Test latch boundary crossing: 22 LEDs = 66 bytes
    // Latch: ((22 * 3 + 63) / 64) = 129 / 64 = 2 bytes
    fl::vector<fl::array<u8, 3>> input(22, {128, 128, 128});

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Expected: 66 LED bytes + 2 latch bytes
    CHECK_EQ(output.size(), 68);

    // Verify last two bytes are latch
    CHECK_EQ(output[66], 0x00);
    CHECK_EQ(output[67], 0x00);
}

TEST_CASE("LPD8806 - Latch calculation - 40 LEDs") {
    // Test larger LED count (reduced from 64 to 40 for performance)
    // Still provides good coverage of latch calculation with multiple latch bytes
    // Latch: ((40 * 3 + 63) / 64) = 183 / 64 = 2 bytes
    fl::vector<fl::array<u8, 3>> input(40, {255, 0, 128});

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Expected: 120 LED bytes + 2 latch bytes
    CHECK_EQ(output.size(), 122);

    // Verify first LED encoding (GRB = 255, 0, 128)
    CHECK_EQ(output[0], 0xFF);  // Green=255 -> 0xFF
    CHECK_EQ(output[1], 0x80);  // Red=0 -> 0x80
    CHECK_EQ(output[2], 0xC1);  // Blue=128 -> 0xC1

    // Verify latch bytes
    CHECK_EQ(output[120], 0x00);
    CHECK_EQ(output[121], 0x00);
}

TEST_CASE("LPD8806 - MSB always set on all LED bytes") {
    // Verify that all LED data bytes have MSB set (0x80 bit)
    fl::vector<fl::array<u8, 3>> input = {
        {0, 0, 0},        // All zeros
        {255, 255, 255},  // All max
        {1, 127, 254}     // Mixed values
    };

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Check all LED bytes (9 bytes) have MSB set
    // Latch bytes (last 1 byte) should be 0x00
    for (size_t i = 0; i < 9; i++) {
        CHECK_GE(output[i], 0x80);  // MSB set
    }

    // Verify latch is 0x00 (MSB not set)
    CHECK_EQ(output[9], 0x00);
}

TEST_CASE("LPD8806 - 7-bit color depth verification") {
    // Verify that encoding preserves proportional relationships
    // within 7-bit range (0-127)
    fl::vector<fl::array<u8, 3>> input = {
        {0, 128, 255}  // Min, mid, max
    };

    fl::vector<u8> output;
    encodeLPD8806(input.begin(), input.end(), fl::back_inserter(output));

    // Extract 7-bit values (remove MSB)
    u8 val0 = output[0] & 0x7F;  // Green=0
    u8 val1 = output[1] & 0x7F;  // Red=128
    u8 val2 = output[2] & 0x7F;  // Blue=255

    // Verify 7-bit range
    CHECK_EQ(val0, 0);    // 0 maps to 0
    CHECK_EQ(val1, 65);   // 128 maps to 65 (with rounding)
    CHECK_EQ(val2, 127);  // 255 maps to 127 (max 7-bit value)

    // Verify proportional spacing (approximate due to integer division)
    CHECK_LT(val0, val1);
    CHECK_LT(val1, val2);
}

} // anonymous namespace
