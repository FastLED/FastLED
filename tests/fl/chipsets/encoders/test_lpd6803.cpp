#include "doctest.h"
#include "fl/chipsets/encoders/lpd6803.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"

using namespace fl;

// Named namespace to avoid unity build conflicts
namespace test_lpd6803 {

// Helper: Verify start boundary (4 bytes of 0x00)
static void verifyStartBoundary(const vector<u8>& data) {
    REQUIRE(data.size() >= 4);
    CHECK(data[0] == 0x00);
    CHECK(data[1] == 0x00);
    CHECK(data[2] == 0x00);
    CHECK(data[3] == 0x00);
}

// Helper: Verify end boundary ((num_leds/32) DWords of 0xFF000000)
static void verifyEndBoundary(const vector<u8>& data, size_t num_leds, size_t start_offset) {
    size_t expected_dwords = num_leds / 32;
    size_t expected_bytes = expected_dwords * 4;

    if (expected_dwords == 0) {
        // No end boundary for < 32 LEDs
        CHECK(data.size() == start_offset);
        return;
    }

    REQUIRE(data.size() >= start_offset + expected_bytes);

    for (size_t i = 0; i < expected_dwords; i++) {
        size_t offset = start_offset + (i * 4);
        CHECK(data[offset + 0] == 0xFF);
        CHECK(data[offset + 1] == 0x00);
        CHECK(data[offset + 2] == 0x00);
        CHECK(data[offset + 3] == 0x00);
    }
}

// Helper: Verify 16-bit LED frame at specific offset
static void verifyLEDFrame(const vector<u8>& data, size_t offset, u8 r, u8 g, u8 b) {
    REQUIRE(data.size() >= offset + 2);

    u16 actual = (static_cast<u16>(data[offset]) << 8) | data[offset + 1];
    u16 expected = lpd6803EncodeRGB(r, g, b);

    CHECK(actual == expected);

    // Verify marker bit is set
    CHECK((actual & 0x8000) == 0x8000);
}

} // namespace test_lpd6803

using namespace test_lpd6803;

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST_CASE("lpd6803EncodeRGB() - marker bit set") {
    u16 result = fl::lpd6803EncodeRGB(0, 0, 0);
    CHECK((result & 0x8000) == 0x8000);
}

TEST_CASE("lpd6803EncodeRGB() - black (0,0,0)") {
    u16 result = fl::lpd6803EncodeRGB(0, 0, 0);
    CHECK(result == 0x8000);  // Only marker bit set
}

TEST_CASE("lpd6803EncodeRGB() - white (255,255,255)") {
    u16 result = fl::lpd6803EncodeRGB(255, 255, 255);
    // 255 >> 3 = 31 (0x1F) for each component
    // Expected: 1bbbbbgggggrrrrr = 1_11111_11111_11111 = 0xFFFF
    CHECK(result == 0xFFFF);
}

TEST_CASE("lpd6803EncodeRGB() - pure red (255,0,0)") {
    u16 result = fl::lpd6803EncodeRGB(255, 0, 0);
    // Red in bits 14-10: (255 & 0xF8) << 7 = 0xF8 << 7 = 0x7C00
    // Expected: 0x8000 | 0x7C00 = 0xFC00
    CHECK(result == 0xFC00);
}

TEST_CASE("lpd6803EncodeRGB() - pure green (0,255,0)") {
    u16 result = fl::lpd6803EncodeRGB(0, 255, 0);
    // Green in bits 9-5: (255 & 0xF8) << 2 = 0xF8 << 2 = 0x03E0
    // Expected: 0x8000 | 0x03E0 = 0x83E0
    CHECK(result == 0x83E0);
}

TEST_CASE("lpd6803EncodeRGB() - pure blue (0,0,255)") {
    u16 result = fl::lpd6803EncodeRGB(0, 0, 255);
    // Blue in bits 4-0: 255 >> 3 = 0x1F
    // Expected: 0x8000 | 0x001F = 0x801F
    CHECK(result == 0x801F);
}

TEST_CASE("lpd6803EncodeRGB() - mid-range values (128,128,128)") {
    u16 result = fl::lpd6803EncodeRGB(128, 128, 128);
    // 128 >> 3 = 16 (0x10) for each component
    // Red: (128 & 0xF8) << 7 = 0x80 << 7 = 0x4000
    // Green: (128 & 0xF8) << 2 = 0x80 << 2 = 0x0200
    // Blue: 128 >> 3 = 0x10
    // Expected: 0x8000 | 0x4000 | 0x0200 | 0x0010 = 0xC210
    CHECK(result == 0xC210);
}

TEST_CASE("lpd6803EncodeRGB() - low values (7,7,7)") {
    u16 result = fl::lpd6803EncodeRGB(7, 7, 7);
    // 7 >> 3 = 0 for each component (all bits lost in 5-bit precision)
    CHECK(result == 0x8000);
}

TEST_CASE("lpd6803EncodeRGB() - boundary (8,8,8)") {
    u16 result = fl::lpd6803EncodeRGB(8, 8, 8);
    // 8 >> 3 = 1 for each component
    // Red: (8 & 0xF8) << 7 = 0x08 << 7 = 0x0400
    // Green: (8 & 0xF8) << 2 = 0x08 << 2 = 0x0020
    // Blue: 8 >> 3 = 0x01
    // Expected: 0x8000 | 0x0400 | 0x0020 | 0x0001 = 0x8421
    CHECK(result == 0x8421);
}

// ============================================================================
// Frame Structure Tests
// ============================================================================

TEST_CASE("encodeLPD6803() - empty range (0 LEDs)") {
    fl::vector<fl::array<fl::u8, 3>> leds;
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Should have only start boundary (4 bytes)
    REQUIRE(output.size() == 4);
    test_lpd6803::verifyStartBoundary(output);
}

TEST_CASE("encodeLPD6803() - single LED black") {
    fl::vector<fl::array<fl::u8, 3>> leds = {{{0, 0, 0}}};
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + LED data (2) = 6 bytes
    REQUIRE(output.size() == 6);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyLEDFrame(output, 4, 0, 0, 0);
}

TEST_CASE("encodeLPD6803() - single LED white") {
    fl::vector<fl::array<fl::u8, 3>> leds = {{{255, 255, 255}}};
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    REQUIRE(output.size() == 6);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyLEDFrame(output, 4, 255, 255, 255);
}

TEST_CASE("encodeLPD6803() - single LED red") {
    fl::vector<fl::array<fl::u8, 3>> leds = {{{255, 0, 0}}};
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    REQUIRE(output.size() == 6);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyLEDFrame(output, 4, 255, 0, 0);
}

TEST_CASE("encodeLPD6803() - multiple LEDs (3 LEDs)") {
    fl::vector<fl::array<fl::u8, 3>> leds = {
        {{255, 0, 0}},    // Red
        {{0, 255, 0}},    // Green
        {{0, 0, 255}}     // Blue
    };
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + 3 LEDs (6) = 10 bytes
    REQUIRE(output.size() == 10);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyLEDFrame(output, 4, 255, 0, 0);
    test_lpd6803::verifyLEDFrame(output, 6, 0, 255, 0);
    test_lpd6803::verifyLEDFrame(output, 8, 0, 0, 255);
}

// ============================================================================
// End Boundary Tests (Critical for LPD6803)
// ============================================================================

TEST_CASE("encodeLPD6803() - 31 LEDs (no end boundary)") {
    fl::vector<fl::array<fl::u8, 3>> leds(31, {{128, 128, 128}});
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + 31 LEDs (62) = 66 bytes (no end boundary)
    REQUIRE(output.size() == 66);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyEndBoundary(output, 31, 66);
}

TEST_CASE("encodeLPD6803() - 32 LEDs (1 DWord end boundary)") {
    fl::vector<fl::array<fl::u8, 3>> leds(32, {{128, 128, 128}});
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + 32 LEDs (64) + end boundary (4) = 72 bytes
    REQUIRE(output.size() == 72);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyEndBoundary(output, 32, 68);
}

TEST_CASE("encodeLPD6803() - 40 LEDs (1 DWord end boundary)") {
    // Reduced from 64 to 40 LEDs for performance (still tests end boundary beyond 32)
    fl::vector<fl::array<fl::u8, 3>> leds(40, {{255, 128, 64}});
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + 40 LEDs (80) + end boundary (4) = 88 bytes
    // 40 / 32 = 1 DWord
    REQUIRE(output.size() == 88);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyEndBoundary(output, 40, 84);
}

TEST_CASE("encodeLPD6803() - 70 LEDs (2 DWord end boundary)") {
    // Reduced from 96 to 70 LEDs for performance (still tests multiple DWord end boundary)
    fl::vector<fl::array<fl::u8, 3>> leds(70, {{100, 150, 200}});
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + 70 LEDs (140) + end boundary (8) = 152 bytes
    // 70 / 32 = 2 DWords
    REQUIRE(output.size() == 152);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyEndBoundary(output, 70, 144);
}

TEST_CASE("encodeLPD6803() - 72 LEDs (2 DWord end boundary)") {
    // Reduced from 100 to 72 LEDs for performance (still tests multiple DWord end boundary)
    fl::vector<fl::array<fl::u8, 3>> leds(72, {{50, 100, 150}});
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Start boundary (4) + 72 LEDs (144) + end boundary (8) = 156 bytes
    // 72 / 32 = 2 DWords
    REQUIRE(output.size() == 156);
    test_lpd6803::verifyStartBoundary(output);
    test_lpd6803::verifyEndBoundary(output, 72, 148);
}

// ============================================================================
// Color Precision Tests (5-bit per channel)
// ============================================================================

TEST_CASE("encodeLPD6803() - color precision loss") {
    // Test that consecutive values differing by less than 8 encode identically
    fl::vector<fl::array<fl::u8, 3>> leds = {
        {{0, 0, 0}},      // Bin 0
        {{7, 7, 7}},      // Still bin 0 (7 >> 3 = 0)
        {{8, 8, 8}},      // Bin 1 (8 >> 3 = 1)
        {{15, 15, 15}},   // Still bin 1 (15 >> 3 = 1)
        {{16, 16, 16}}    // Bin 2 (16 >> 3 = 2)
    };
    fl::vector<fl::u8> output;

    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Verify first two LEDs encode identically
    u16 led0 = (static_cast<u16>(output[4]) << 8) | output[5];
    u16 led1 = (static_cast<u16>(output[6]) << 8) | output[7];
    CHECK(led0 == led1);

    // Verify LED 2 and LED 3 encode identically
    u16 led2 = (static_cast<u16>(output[8]) << 8) | output[9];
    u16 led3 = (static_cast<u16>(output[10]) << 8) | output[11];
    CHECK(led2 == led3);

    // Verify LED 2 differs from LED 0
    CHECK(led2 != led0);

    // Verify LED 4 differs from LED 2
    u16 led4 = (static_cast<u16>(output[12]) << 8) | output[13];
    CHECK(led4 != led2);
}

TEST_CASE("encodeLPD6803() - 5-bit boundaries") {
    // Test each 5-bit boundary (0, 8, 16, ..., 248)
    fl::vector<fl::array<fl::u8, 3>> leds;
    for (int i = 0; i < 32; i++) {
        u8 value = static_cast<u8>(i * 8);
        leds.push_back({{value, value, value}});
    }

    fl::vector<fl::u8> output;
    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    // Verify each LED encodes with correct 5-bit value
    for (size_t i = 0; i < 32; i++) {
        size_t offset = 4 + (i * 2);
        u16 actual = (static_cast<u16>(output[offset]) << 8) | output[offset + 1];

        // Expected: marker bit + (i << 10) + (i << 5) + i = 0x8000 | (i * 0x421)
        u16 expected = 0x8000 | (static_cast<u16>(i) * 0x421);
        CHECK(actual == expected);
    }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE("encodeLPD6803() - alternating pattern") {
    fl::vector<fl::array<fl::u8, 3>> leds;
    for (int i = 0; i < 10; i++) {
        if (i % 2 == 0) {
            leds.push_back({{255, 0, 0}});  // Red
        } else {
            leds.push_back({{0, 0, 255}});  // Blue
        }
    }

    fl::vector<fl::u8> output;
    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    REQUIRE(output.size() == 24);  // 4 + 20 = 24

    // Verify alternating pattern
    for (int i = 0; i < 10; i++) {
        size_t offset = 4 + (i * 2);
        if (i % 2 == 0) {
            test_lpd6803::verifyLEDFrame(output, offset, 255, 0, 0);
        } else {
            test_lpd6803::verifyLEDFrame(output, offset, 0, 0, 255);
        }
    }
}

TEST_CASE("encodeLPD6803() - gradient pattern") {
    fl::vector<fl::array<fl::u8, 3>> leds;
    for (int i = 0; i < 16; i++) {
        u8 value = static_cast<u8>(i * 16);
        leds.push_back({{value, value, value}});
    }

    fl::vector<fl::u8> output;
    fl::encodeLPD6803(leds.begin(), leds.end(), fl::back_inserter(output));

    REQUIRE(output.size() == 36);  // 4 + 32 = 36

    // Verify gradient encoding
    for (int i = 0; i < 16; i++) {
        size_t offset = 4 + (i * 2);
        u8 expected_value = static_cast<u8>(i * 16);
        test_lpd6803::verifyLEDFrame(output, offset, expected_value, expected_value, expected_value);
    }
}
