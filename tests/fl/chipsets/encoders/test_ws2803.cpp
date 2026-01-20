/// @file test_ws2803.cpp
/// @brief Unit tests for WS2803 LED chipset encoder
///
/// WS2803 Protocol Format:
/// - IDENTICAL to WS2801 protocol
/// - LED data: 3 bytes per LED (RGB order)
/// - No frame overhead (latch is timing-based, not data-based)
/// - Clock speed: typically 25 MHz (controller default, higher than WS2801)
/// - Direct RGB byte streaming in wire order
///
/// Protocol Details:
/// - Wire order: R, G, B (pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue)
/// - No start frame
/// - No end frame
/// - Latching: Occurs via timing (pause in clock signal)
/// - The ONLY difference from WS2801 is the typical clock speed
///
/// Implementation:
/// - encodeWS2803() is a direct alias/wrapper for encodeWS2801()
/// - All protocol behavior is identical
/// - Tests verify that the alias works correctly

#include "fl/chipsets/encoders/ws2803.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "initializer_list"
#include "fl/stl/vector.h"

using namespace fl;

namespace test_ws2803 {

/// Helper to create RGB pixel array in wire order
fl::array<u8, 3> makeRGBPixel(u8 r, u8 g, u8 b) {
    return {r, g, b};
}

/// Helper to verify output contains expected RGB bytes at offset
void verifyRGBAt(const fl::vector<u8>& output, size_t offset, u8 r, u8 g, u8 b) {
    REQUIRE_LT(offset + 2, output.size());
    CHECK_EQ(output[offset + 0], r);  // Red
    CHECK_EQ(output[offset + 1], g);  // Green
    CHECK_EQ(output[offset + 2], b);  // Blue
}

using namespace test_ws2803;

// ============================================================================
// Basic Functionality Tests (verify alias works)
// ============================================================================

TEST_CASE("WS2803 - Zero LEDs (empty input)") {
    // Test encoding with no LEDs - should produce no output
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> output;

    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 0 bytes (no frame overhead)
    REQUIRE_EQ(output.size(), 0);
}

TEST_CASE("WS2803 - Single LED (black)") {
    // Test single black LED
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 0, 0));

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0x00, 0x00, 0x00);
}

TEST_CASE("WS2803 - Single LED (white)") {
    // Test single white LED
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 255, 255));

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0xFF, 0xFF, 0xFF);
}

TEST_CASE("WS2803 - RGB primaries") {
    // Test RGB primary colors to verify wire order
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 0, 0));    // Red
    pixels.push_back(makeRGBPixel(0, 255, 0));    // Green
    pixels.push_back(makeRGBPixel(0, 0, 255));    // Blue

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 9 bytes
    REQUIRE_EQ(output.size(), 9);
    verifyRGBAt(output, 0, 0xFF, 0x00, 0x00);  // Red
    verifyRGBAt(output, 3, 0x00, 0xFF, 0x00);  // Green
    verifyRGBAt(output, 6, 0x00, 0x00, 0xFF);  // Blue
}

TEST_CASE("WS2803 - Multiple LEDs (mixed colors)") {
    // Test various color combinations
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(128, 64, 32));
    pixels.push_back(makeRGBPixel(200, 100, 50));
    pixels.push_back(makeRGBPixel(255, 128, 64));

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 9 bytes
    REQUIRE_EQ(output.size(), 9);
    verifyRGBAt(output, 0, 128, 64, 32);
    verifyRGBAt(output, 3, 200, 100, 50);
    verifyRGBAt(output, 6, 255, 128, 64);
}

// ============================================================================
// Protocol Equivalence Tests (verify identical to WS2801)
// ============================================================================

TEST_CASE("WS2803 - Protocol equivalence to WS2801") {
    // Verify that WS2803 produces identical output to WS2801
    // This is the key test - both encoders should produce the same output
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0xAA, 0xBB, 0xCC));
    pixels.push_back(makeRGBPixel(0x11, 0x22, 0x33));
    pixels.push_back(makeRGBPixel(0xFF, 0x00, 0x80));

    fl::vector<u8> ws2803_output;
    fl::vector<u8> ws2801_output;

    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(ws2803_output));
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(ws2801_output));

    // Outputs should be identical
    REQUIRE_EQ(ws2803_output.size(), ws2801_output.size());
    for (size_t i = 0; i < ws2803_output.size(); ++i) {
        CHECK_EQ(ws2803_output[i], ws2801_output[i]);
    }
}

TEST_CASE("WS2803 - No frame overhead (same as WS2801)") {
    // Verify WS2803 has no frame overhead, just like WS2801
    for (size_t num_leds : {0, 1, 5, 20, 60}) {
        fl::vector<fl::array<u8, 3>> pixels(num_leds, makeRGBPixel(128, 128, 128));
        fl::vector<u8> output;

        encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

        // Expected: num_leds × 3 bytes (no frame overhead)
        CHECK_EQ(output.size(), num_leds * 3);
    }
}

// ============================================================================
// Wire Order Tests
// ============================================================================

TEST_CASE("WS2803 - RGB wire order verification") {
    // Verify bytes are written in RGB order
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0x12, 0x34, 0x56));

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 3);
    CHECK_EQ(output[0], 0x12);  // Red first
    CHECK_EQ(output[1], 0x34);  // Green second
    CHECK_EQ(output[2], 0x56);  // Blue third
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE("WS2803 - Boundary values") {
    // Test minimum and maximum byte values
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 0, 0));
    pixels.push_back(makeRGBPixel(255, 255, 255));

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 6);
    verifyRGBAt(output, 0, 0x00, 0x00, 0x00);
    verifyRGBAt(output, 3, 0xFF, 0xFF, 0xFF);
}

TEST_CASE("WS2803 - Many LEDs (typical strip)") {
    // Test typical strip size (reduced from 60 to 30 for performance)
    // Still provides excellent coverage for typical strip encoding
    const size_t NUM_LEDS = 30;
    fl::vector<fl::array<u8, 3>> pixels(NUM_LEDS, makeRGBPixel(200, 150, 100));

    fl::vector<u8> output;
    encodeWS2803(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: NUM_LEDS × 3 bytes
    REQUIRE_EQ(output.size(), NUM_LEDS * 3);

    // Verify all LEDs have consistent encoding
    for (size_t i = 0; i < NUM_LEDS; ++i) {
        verifyRGBAt(output, i * 3, 200, 150, 100);
    }
}

} // namespace test_ws2803
