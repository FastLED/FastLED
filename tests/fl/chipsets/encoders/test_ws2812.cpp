/// @file test_ws2812.cpp
/// @brief Unit tests for WS2812 LED chipset encoder
///
/// WS2812 Protocol Format:
/// - RGB mode: 3 bytes per LED (no frame overhead)
/// - RGBW mode: 4 bytes per LED (no frame overhead)
/// - No start/end frames (timing-based protocol)
/// - Direct byte streaming in wire order
///
/// This is the simplest encoder - just copies bytes from input to output.

#include "fl/chipsets/encoders/ws2812.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"

using namespace fl;

namespace {

/// Helper to create RGB pixel array
fl::array<u8, 3> makeRGBPixel(u8 r, u8 g, u8 b) {
    return {r, g, b};
}

/// Helper to create RGBW pixel array
fl::array<u8, 4> makeRGBWPixel(u8 r, u8 g, u8 b, u8 w) {
    return {r, g, b, w};
}

// ============================================================================
// RGB Mode Tests
// ============================================================================

TEST_CASE("WS2812_RGB - Zero LEDs (empty input)") {
    // Test encoding with no LEDs - should produce no output
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> output;

    encodeWS2812_RGB(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 0 bytes (no frame overhead for WS2812)
    FL_REQUIRE_EQ(output.size(), 0);
}

TEST_CASE("WS2812_RGB - Single LED (black)") {
    // Test single black LED (0,0,0)
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(0, 0, 0));

    fl::vector<u8> output;
    encodeWS2812_RGB(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes
    FL_REQUIRE_EQ(output.size(), 3);
    FL_CHECK_EQ(output[0], 0x00);  // First byte
    FL_CHECK_EQ(output[1], 0x00);  // Second byte
    FL_CHECK_EQ(output[2], 0x00);  // Third byte
}

TEST_CASE("WS2812_RGB - Single LED (white)") {
    // Test single white LED (255,255,255)
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 255, 255));

    fl::vector<u8> output;
    encodeWS2812_RGB(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes
    FL_REQUIRE_EQ(output.size(), 3);
    FL_CHECK_EQ(output[0], 0xFF);
    FL_CHECK_EQ(output[1], 0xFF);
    FL_CHECK_EQ(output[2], 0xFF);
}

TEST_CASE("WS2812_RGB - Single LED (red)") {
    // Test single red LED - verify byte order preservation
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 0, 0));

    fl::vector<u8> output;
    encodeWS2812_RGB(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 3 bytes, first byte should be 255
    FL_REQUIRE_EQ(output.size(), 3);
    FL_CHECK_EQ(output[0], 0xFF);  // Red
    FL_CHECK_EQ(output[1], 0x00);  // Green
    FL_CHECK_EQ(output[2], 0x00);  // Blue
}

TEST_CASE("WS2812_RGB - Multiple LEDs") {
    // Test multiple LEDs to verify iteration works correctly
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeRGBPixel(255, 0, 0));    // Red
    pixels.push_back(makeRGBPixel(0, 255, 0));    // Green
    pixels.push_back(makeRGBPixel(0, 0, 255));    // Blue
    pixels.push_back(makeRGBPixel(128, 64, 32));  // Mixed

    fl::vector<u8> output;
    encodeWS2812_RGB(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 LEDs × 3 bytes = 12 bytes
    FL_REQUIRE_EQ(output.size(), 12);

    // LED 0: Red
    FL_CHECK_EQ(output[0], 255);
    FL_CHECK_EQ(output[1], 0);
    FL_CHECK_EQ(output[2], 0);

    // LED 1: Green
    FL_CHECK_EQ(output[3], 0);
    FL_CHECK_EQ(output[4], 255);
    FL_CHECK_EQ(output[5], 0);

    // LED 2: Blue
    FL_CHECK_EQ(output[6], 0);
    FL_CHECK_EQ(output[7], 0);
    FL_CHECK_EQ(output[8], 255);

    // LED 3: Mixed
    FL_CHECK_EQ(output[9], 128);
    FL_CHECK_EQ(output[10], 64);
    FL_CHECK_EQ(output[11], 32);
}

TEST_CASE("WS2812_RGB - Many LEDs (stress test)") {
    // Test with a larger number of LEDs to ensure iteration is robust
    // Reduced from 100 to 40 for performance (still provides good stress test coverage)
    const size_t NUM_LEDS = 40;
    fl::vector<fl::array<u8, 3>> pixels;

    for (size_t i = 0; i < NUM_LEDS; i++) {
        u8 val = static_cast<u8>(i % 256);
        pixels.push_back(makeRGBPixel(val, val + 1, val + 2));
    }

    fl::vector<u8> output;
    encodeWS2812_RGB(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 40 LEDs × 3 bytes = 120 bytes
    FL_REQUIRE_EQ(output.size(), 120);

    // Verify first and last LEDs
    FL_CHECK_EQ(output[0], 0);     // First LED R
    FL_CHECK_EQ(output[1], 1);     // First LED G
    FL_CHECK_EQ(output[2], 2);     // First LED B

    u8 last_val = static_cast<u8>((NUM_LEDS - 1) % 256);
    FL_CHECK_EQ(output[117], last_val);       // Last LED R
    FL_CHECK_EQ(output[118], last_val + 1);   // Last LED G
    FL_CHECK_EQ(output[119], last_val + 2);   // Last LED B
}

// ============================================================================
// RGBW Mode Tests
// ============================================================================

TEST_CASE("WS2812_RGBW - Zero LEDs (empty input)") {
    // Test encoding with no LEDs - should produce no output
    fl::vector<fl::array<u8, 4>> pixels;
    fl::vector<u8> output;

    encodeWS2812_RGBW(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 0 bytes (no frame overhead for WS2812)
    FL_REQUIRE_EQ(output.size(), 0);
}

TEST_CASE("WS2812_RGBW - Single LED (black)") {
    // Test single black LED (0,0,0,0)
    fl::vector<fl::array<u8, 4>> pixels;
    pixels.push_back(makeRGBWPixel(0, 0, 0, 0));

    fl::vector<u8> output;
    encodeWS2812_RGBW(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 bytes
    FL_REQUIRE_EQ(output.size(), 4);
    FL_CHECK_EQ(output[0], 0x00);
    FL_CHECK_EQ(output[1], 0x00);
    FL_CHECK_EQ(output[2], 0x00);
    FL_CHECK_EQ(output[3], 0x00);
}

TEST_CASE("WS2812_RGBW - Single LED (white via W channel)") {
    // Test white LED using W channel
    fl::vector<fl::array<u8, 4>> pixels;
    pixels.push_back(makeRGBWPixel(0, 0, 0, 255));

    fl::vector<u8> output;
    encodeWS2812_RGBW(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 bytes
    FL_REQUIRE_EQ(output.size(), 4);
    FL_CHECK_EQ(output[0], 0x00);  // R
    FL_CHECK_EQ(output[1], 0x00);  // G
    FL_CHECK_EQ(output[2], 0x00);  // B
    FL_CHECK_EQ(output[3], 0xFF);  // W
}

TEST_CASE("WS2812_RGBW - Single LED (all channels active)") {
    // Test with all channels active
    fl::vector<fl::array<u8, 4>> pixels;
    pixels.push_back(makeRGBWPixel(255, 128, 64, 32));

    fl::vector<u8> output;
    encodeWS2812_RGBW(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 bytes
    FL_REQUIRE_EQ(output.size(), 4);
    FL_CHECK_EQ(output[0], 255);  // R
    FL_CHECK_EQ(output[1], 128);  // G
    FL_CHECK_EQ(output[2], 64);   // B
    FL_CHECK_EQ(output[3], 32);   // W
}

TEST_CASE("WS2812_RGBW - Multiple LEDs") {
    // Test multiple RGBW LEDs
    fl::vector<fl::array<u8, 4>> pixels;
    pixels.push_back(makeRGBWPixel(255, 0, 0, 0));    // Red
    pixels.push_back(makeRGBWPixel(0, 255, 0, 0));    // Green
    pixels.push_back(makeRGBWPixel(0, 0, 255, 0));    // Blue
    pixels.push_back(makeRGBWPixel(0, 0, 0, 255));    // White
    pixels.push_back(makeRGBWPixel(128, 64, 32, 16)); // Mixed

    fl::vector<u8> output;
    encodeWS2812_RGBW(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 5 LEDs × 4 bytes = 20 bytes
    FL_REQUIRE_EQ(output.size(), 20);

    // LED 0: Red
    FL_CHECK_EQ(output[0], 255);
    FL_CHECK_EQ(output[1], 0);
    FL_CHECK_EQ(output[2], 0);
    FL_CHECK_EQ(output[3], 0);

    // LED 1: Green
    FL_CHECK_EQ(output[4], 0);
    FL_CHECK_EQ(output[5], 255);
    FL_CHECK_EQ(output[6], 0);
    FL_CHECK_EQ(output[7], 0);

    // LED 2: Blue
    FL_CHECK_EQ(output[8], 0);
    FL_CHECK_EQ(output[9], 0);
    FL_CHECK_EQ(output[10], 255);
    FL_CHECK_EQ(output[11], 0);

    // LED 3: White
    FL_CHECK_EQ(output[12], 0);
    FL_CHECK_EQ(output[13], 0);
    FL_CHECK_EQ(output[14], 0);
    FL_CHECK_EQ(output[15], 255);

    // LED 4: Mixed
    FL_CHECK_EQ(output[16], 128);
    FL_CHECK_EQ(output[17], 64);
    FL_CHECK_EQ(output[18], 32);
    FL_CHECK_EQ(output[19], 16);
}

TEST_CASE("WS2812_RGBW - Many LEDs (stress test)") {
    // Test with a larger number of LEDs
    // Reduced from 100 to 40 for performance (still provides good stress test coverage)
    const size_t NUM_LEDS = 40;
    fl::vector<fl::array<u8, 4>> pixels;

    for (size_t i = 0; i < NUM_LEDS; i++) {
        u8 val = static_cast<u8>(i % 256);
        pixels.push_back(makeRGBWPixel(val, val + 1, val + 2, val + 3));
    }

    fl::vector<u8> output;
    encodeWS2812_RGBW(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 40 LEDs × 4 bytes = 160 bytes
    FL_REQUIRE_EQ(output.size(), 160);

    // Verify first and last LEDs
    FL_CHECK_EQ(output[0], 0);     // First LED R
    FL_CHECK_EQ(output[1], 1);     // First LED G
    FL_CHECK_EQ(output[2], 2);     // First LED B
    FL_CHECK_EQ(output[3], 3);     // First LED W

    u8 last_val = static_cast<u8>((NUM_LEDS - 1) % 256);
    FL_CHECK_EQ(output[156], last_val);       // Last LED R
    FL_CHECK_EQ(output[157], last_val + 1);   // Last LED G
    FL_CHECK_EQ(output[158], last_val + 2);   // Last LED B
    FL_CHECK_EQ(output[159], last_val + 3);   // Last LED W
}

// ============================================================================
// Note: The encodeWS2812() dispatch function exists but requires the input
// iterator type to match the pixel size (3-byte vs 4-byte arrays). It's
// primarily used internally by pixel_iterator.h, not as a standalone API.
// Direct testing would require complex template instantiation, so we test
// the RGB and RGBW encoders separately above.
// ============================================================================

} // anonymous namespace
