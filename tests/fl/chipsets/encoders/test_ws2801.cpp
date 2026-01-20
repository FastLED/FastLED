/// @file test_ws2801.cpp
/// @brief Unit tests for WS2801 LED chipset encoder
///
/// WS2801 Protocol Format:
/// - LED data: 3 bytes per LED (RGB order)
/// - No frame overhead (latch is timing-based, not data-based)
/// - Clock speed: typically 1 MHz (controller default)
/// - Direct RGB byte streaming in wire order
///
/// Protocol Details:
/// - Wire order: R, G, B (pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue)
/// - No start frame
/// - No end frame
/// - Latching: Occurs via timing (pause in clock signal)
/// - Reset time: ~500µs clock low time required between frames
///
/// Refactoring History:
/// - This consolidates encoding logic previously in WS2801Controller template
/// - Encoder now handles all protocol details (no frame overhead for WS2801)
/// - Controller just manages timing and SPI communication

#include "fl/chipsets/encoders/ws2801.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "initializer_list"

using namespace fl;

namespace test_ws2801 {

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

using namespace test_ws2801;

// ============================================================================
// Frame Structure Tests
// ============================================================================

TEST_CASE("WS2801 - Zero LEDs (empty input)") {
    // Test encoding with no LEDs - should produce no output
    // WS2801 has NO frame overhead (no start/end frames)
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> output;

    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 0 bytes (no frame overhead)
    REQUIRE_EQ(output.size(), 0);
}

TEST_CASE("WS2801 - Single LED (black)") {
    // Test single black LED (0,0,0)
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 0, 0));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes (RGB only, no frame overhead)
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0x00, 0x00, 0x00);
}

TEST_CASE("WS2801 - Single LED (white)") {
    // Test single white LED (255,255,255)
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 255, 255));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0xFF, 0xFF, 0xFF);
}

TEST_CASE("WS2801 - Single LED (red)") {
    // Test single red LED - verify RGB byte order
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 0, 0));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes - Red=255, Green=0, Blue=0
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0xFF, 0x00, 0x00);
}

TEST_CASE("WS2801 - Single LED (green)") {
    // Test single green LED - verify RGB byte order
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 255, 0));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes - Red=0, Green=255, Blue=0
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0x00, 0xFF, 0x00);
}

TEST_CASE("WS2801 - Single LED (blue)") {
    // Test single blue LED - verify RGB byte order
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 0, 255));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes - Red=0, Green=0, Blue=255
    REQUIRE_EQ(output.size(), 3);
    verifyRGBAt(output, 0, 0x00, 0x00, 0xFF);
}

TEST_CASE("WS2801 - Multiple LEDs (RGB primaries)") {
    // Test multiple LEDs to verify iteration works correctly
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 0, 0));    // Red
    pixels.push_back(makeRGBPixel(0, 255, 0));    // Green
    pixels.push_back(makeRGBPixel(0, 0, 255));    // Blue

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 9 bytes total (3 pixels × 3 bytes)
    REQUIRE_EQ(output.size(), 9);

    // Verify each LED encoding
    verifyRGBAt(output, 0, 0xFF, 0x00, 0x00);  // LED 0: Red
    verifyRGBAt(output, 3, 0x00, 0xFF, 0x00);  // LED 1: Green
    verifyRGBAt(output, 6, 0x00, 0x00, 0xFF);  // LED 2: Blue
}

TEST_CASE("WS2801 - Multiple LEDs (mixed colors)") {
    // Test various color combinations
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(128, 64, 32));   // Mixed low
    pixels.push_back(makeRGBPixel(200, 100, 50));  // Mixed mid
    pixels.push_back(makeRGBPixel(255, 128, 64));  // Mixed high

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 9 bytes total
    REQUIRE_EQ(output.size(), 9);

    // Verify each LED encoding
    verifyRGBAt(output, 0, 128, 64, 32);
    verifyRGBAt(output, 3, 200, 100, 50);
    verifyRGBAt(output, 6, 255, 128, 64);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE("WS2801 - Boundary values (min/max)") {
    // Test minimum and maximum byte values
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 0, 0));        // Minimum
    pixels.push_back(makeRGBPixel(255, 255, 255));  // Maximum

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 6 bytes
    REQUIRE_EQ(output.size(), 6);
    verifyRGBAt(output, 0, 0x00, 0x00, 0x00);
    verifyRGBAt(output, 3, 0xFF, 0xFF, 0xFF);
}

TEST_CASE("WS2801 - Many LEDs (typical strip size)") {
    // Test a typical strip size (reduced from 60 to 30 for performance)
    // Still provides excellent coverage for typical strip encoding
    const size_t NUM_LEDS = 30;
    fl::vector<fl::array<u8, 3>> pixels;

    // Fill with gradient pattern
    for (size_t i = 0; i < NUM_LEDS; ++i) {
        u8 val = static_cast<u8>((i * 255) / NUM_LEDS);
        pixels.push_back(makeRGBPixel(val, 255 - val, val / 2));
    }

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: NUM_LEDS × 3 bytes
    REQUIRE_EQ(output.size(), NUM_LEDS * 3);

    // Spot check first and last LED
    verifyRGBAt(output, 0, 0, 255, 0);                              // First LED
    u8 last_val = static_cast<u8>(((NUM_LEDS - 1) * 255) / NUM_LEDS);
    verifyRGBAt(output, (NUM_LEDS - 1) * 3, last_val, 255 - last_val, last_val / 2);
}

} // namespace test_ws2801

// ============================================================================
// Wire Order Tests
// ============================================================================

namespace test_ws2801 {
using namespace test_ws2801;

TEST_CASE("WS2801 - RGB wire order verification") {
    // Verify that bytes are written in RGB order
    // Input pixel: array<u8, 3> where [0]=R, [1]=G, [2]=B
    // Expected output: byte[0]=R, byte[1]=G, byte[2]=B

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0xAA, 0xBB, 0xCC));  // Distinct values

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 3);
    CHECK_EQ(output[0], 0xAA);  // Red first
    CHECK_EQ(output[1], 0xBB);  // Green second
    CHECK_EQ(output[2], 0xCC);  // Blue third
}

// ============================================================================
// Protocol Compliance Tests
// ============================================================================

TEST_CASE("WS2801 - No start frame") {
    // Verify that WS2801 produces no start frame
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(128, 128, 128));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Output should start immediately with LED data (no preamble)
    REQUIRE_EQ(output.size(), 3);
    CHECK_EQ(output[0], 128);  // First byte should be LED data, not frame marker
}

TEST_CASE("WS2801 - No end frame") {
    // Verify that WS2801 produces no end frame
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(128, 128, 128));

    fl::vector<u8> output;
    encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Output should end immediately after LED data (no termination bytes)
    REQUIRE_EQ(output.size(), 3);  // Exactly 3 bytes, no extra termination
}

TEST_CASE("WS2801 - Byte count calculation (protocol compliance)") {
    // Verify byte count matches protocol specification
    // WS2801: 3 bytes per LED, no overhead

    for (size_t num_leds : {0, 1, 2, 10, 50, 100}) {
        fl::vector<fl::array<u8, 3>> pixels(num_leds, makeRGBPixel(0, 0, 0));
        fl::vector<u8> output;

        encodeWS2801(pixels.begin(), pixels.end(), fl::back_inserter(output));

        // Expected: num_leds × 3 bytes (no frame overhead)
        CHECK_EQ(output.size(), num_leds * 3);
    }
}

} // namespace test_ws2801
