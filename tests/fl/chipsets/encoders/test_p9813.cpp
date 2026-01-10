/// @file test_p9813.cpp
/// @brief Unit tests for P9813 SPI LED chipset encoder
///
/// P9813 Protocol Format:
/// - Start boundary: 4 bytes of 0x00
/// - LED frames: 4 bytes per LED [Flag][B][G][R]
///   * Flag byte: 0xC0 | checksum
///   * Checksum uses inverted top 2 bits of RGB
/// - End boundary: 4 bytes of 0x00
/// - Wire order: BGR (pixel[0]=Blue, pixel[1]=Green, pixel[2]=Red)
///
/// Flag Byte Calculation:
/// flag = 0xC0 | (~b & 0xC0) >> 2 | (~g & 0xC0) >> 4 | (~r & 0xC0) >> 6

#include "fl/chipsets/encoders/p9813.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"

using namespace fl;

namespace {

/// Helper to calculate expected P9813 flag byte from RGB values
u8 expectedFlagByte(u8 r, u8 g, u8 b) {
    return 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
}

/// Helper to create BGR pixel array from RGB values
fl::array<u8, 3> makeBGRPixel(u8 r, u8 g, u8 b) {
    return {b, g, r};  // Wire order: BGR
}

TEST_CASE("P9813 - Zero LEDs (empty input)") {
    // Test encoding with no LEDs - should only have start and end boundaries
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> output;

    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 bytes start + 4 bytes end = 8 bytes total
    REQUIRE_EQ(output.size(), 8);

    // Start boundary: 4 bytes of 0x00
    CHECK_EQ(output[0], 0x00);
    CHECK_EQ(output[1], 0x00);
    CHECK_EQ(output[2], 0x00);
    CHECK_EQ(output[3], 0x00);

    // End boundary: 4 bytes of 0x00
    CHECK_EQ(output[4], 0x00);
    CHECK_EQ(output[5], 0x00);
    CHECK_EQ(output[6], 0x00);
    CHECK_EQ(output[7], 0x00);
}

TEST_CASE("P9813 - Single LED (all black)") {
    // Test single black LED (RGB = 0,0,0)
    // All bits inverted for checksum: 0xFF for each channel
    // Checksum: (~0 & 0xC0) >> 2 | (~0 & 0xC0) >> 4 | (~0 & 0xC0) >> 6
    //         = 0x30 | 0x0C | 0x03 = 0x3F
    // Flag: 0xC0 | 0x3F = 0xFF

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(0, 0, 0));

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 (start) + 4 (LED) + 4 (end) = 12 bytes
    REQUIRE_EQ(output.size(), 12);

    // Start boundary
    CHECK_EQ(output[0], 0x00);
    CHECK_EQ(output[1], 0x00);
    CHECK_EQ(output[2], 0x00);
    CHECK_EQ(output[3], 0x00);

    // LED data: [Flag][B][G][R]
    CHECK_EQ(output[4], 0xFF);  // Flag byte
    CHECK_EQ(output[5], 0x00);  // Blue
    CHECK_EQ(output[6], 0x00);  // Green
    CHECK_EQ(output[7], 0x00);  // Red

    // End boundary
    CHECK_EQ(output[8], 0x00);
    CHECK_EQ(output[9], 0x00);
    CHECK_EQ(output[10], 0x00);
    CHECK_EQ(output[11], 0x00);
}

TEST_CASE("P9813 - Single LED (all white)") {
    // Test single white LED (RGB = 255,255,255)
    // All bits set for checksum: 0x00 after inversion
    // Checksum: (~255 & 0xC0) >> 2 | (~255 & 0xC0) >> 4 | (~255 & 0xC0) >> 6
    //         = 0x00 | 0x00 | 0x00 = 0x00
    // Flag: 0xC0 | 0x00 = 0xC0

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(255, 255, 255));

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 12);

    // LED data: [Flag][B][G][R]
    CHECK_EQ(output[4], 0xC0);  // Flag byte
    CHECK_EQ(output[5], 0xFF);  // Blue
    CHECK_EQ(output[6], 0xFF);  // Green
    CHECK_EQ(output[7], 0xFF);  // Red
}

TEST_CASE("P9813 - Single LED (pure red)") {
    // Test pure red LED (RGB = 255,0,0)
    // R=255 (top 2 bits: 11): (~255 & 0xC0) >> 6 = 0x00
    // G=0   (top 2 bits: 00): (~0 & 0xC0) >> 4 = 0x0C
    // B=0   (top 2 bits: 00): (~0 & 0xC0) >> 2 = 0x30
    // Checksum: 0x00 | 0x0C | 0x30 = 0x3C
    // Flag: 0xC0 | 0x3C = 0xFC

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(255, 0, 0));

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 12);

    u8 expectedFlag = expectedFlagByte(255, 0, 0);
    CHECK_EQ(expectedFlag, 0xFC);

    // LED data: [Flag][B][G][R]
    CHECK_EQ(output[4], expectedFlag);  // Flag byte
    CHECK_EQ(output[5], 0x00);          // Blue
    CHECK_EQ(output[6], 0x00);          // Green
    CHECK_EQ(output[7], 0xFF);          // Red
}

TEST_CASE("P9813 - Single LED (pure green)") {
    // Test pure green LED (RGB = 0,255,0)
    // R=0   (top 2 bits: 00): (~0 & 0xC0) >> 6 = 0x03
    // G=255 (top 2 bits: 11): (~255 & 0xC0) >> 4 = 0x00
    // B=0   (top 2 bits: 00): (~0 & 0xC0) >> 2 = 0x30
    // Checksum: 0x03 | 0x00 | 0x30 = 0x33
    // Flag: 0xC0 | 0x33 = 0xF3

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(0, 255, 0));

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 12);

    u8 expectedFlag = expectedFlagByte(0, 255, 0);
    CHECK_EQ(expectedFlag, 0xF3);

    // LED data: [Flag][B][G][R]
    CHECK_EQ(output[4], expectedFlag);  // Flag byte
    CHECK_EQ(output[5], 0x00);          // Blue
    CHECK_EQ(output[6], 0xFF);          // Green
    CHECK_EQ(output[7], 0x00);          // Red
}

TEST_CASE("P9813 - Single LED (pure blue)") {
    // Test pure blue LED (RGB = 0,0,255)
    // R=0   (top 2 bits: 00): (~0 & 0xC0) >> 6 = 0x03
    // G=0   (top 2 bits: 00): (~0 & 0xC0) >> 4 = 0x0C
    // B=255 (top 2 bits: 11): (~255 & 0xC0) >> 2 = 0x00
    // Checksum: 0x03 | 0x0C | 0x00 = 0x0F
    // Flag: 0xC0 | 0x0F = 0xCF

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(0, 0, 255));

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 12);

    u8 expectedFlag = expectedFlagByte(0, 0, 255);
    CHECK_EQ(expectedFlag, 0xCF);

    // LED data: [Flag][B][G][R]
    CHECK_EQ(output[4], expectedFlag);  // Flag byte
    CHECK_EQ(output[5], 0xFF);          // Blue
    CHECK_EQ(output[6], 0x00);          // Green
    CHECK_EQ(output[7], 0x00);          // Red
}

TEST_CASE("P9813 - Checksum boundary values") {
    // Test checksum calculation with various top 2-bit patterns
    SUBCASE("RGB = 64,64,64 (top 2 bits all 01)") {
        // R=64  (0b01000000, top 2: 01): (~64 & 0xC0) >> 6 = 0b10 >> 6 = 0x02
        // G=64  (0b01000000, top 2: 01): (~64 & 0xC0) >> 4 = 0b10 >> 4 = 0x08
        // B=64  (0b01000000, top 2: 01): (~64 & 0xC0) >> 2 = 0b10 >> 2 = 0x20
        // Checksum: 0x02 | 0x08 | 0x20 = 0x2A
        // Flag: 0xC0 | 0x2A = 0xEA

        fl::vector<fl::array<u8, 3>> pixels;
        pixels.push_back(makeBGRPixel(64, 64, 64));

        fl::vector<u8> output;
        encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

        u8 expectedFlag = expectedFlagByte(64, 64, 64);
        CHECK_EQ(expectedFlag, 0xEA);
        CHECK_EQ(output[4], expectedFlag);
    }

    SUBCASE("RGB = 128,128,128 (top 2 bits all 10)") {
        // R=128 (0b10000000, top 2: 10): (~128 & 0xC0) >> 6 = 0b01 >> 6 = 0x01
        // G=128 (0b10000000, top 2: 10): (~128 & 0xC0) >> 4 = 0b01 >> 4 = 0x04
        // B=128 (0b10000000, top 2: 10): (~128 & 0xC0) >> 2 = 0b01 >> 2 = 0x10
        // Checksum: 0x01 | 0x04 | 0x10 = 0x15
        // Flag: 0xC0 | 0x15 = 0xD5

        fl::vector<fl::array<u8, 3>> pixels;
        pixels.push_back(makeBGRPixel(128, 128, 128));

        fl::vector<u8> output;
        encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

        u8 expectedFlag = expectedFlagByte(128, 128, 128);
        CHECK_EQ(expectedFlag, 0xD5);
        CHECK_EQ(output[4], expectedFlag);
    }

    SUBCASE("RGB = 192,192,192 (top 2 bits all 11)") {
        // R=192 (0b11000000, top 2: 11): (~192 & 0xC0) >> 6 = 0x00
        // G=192 (0b11000000, top 2: 11): (~192 & 0xC0) >> 4 = 0x00
        // B=192 (0b11000000, top 2: 11): (~192 & 0xC0) >> 2 = 0x00
        // Checksum: 0x00 | 0x00 | 0x00 = 0x00
        // Flag: 0xC0 | 0x00 = 0xC0

        fl::vector<fl::array<u8, 3>> pixels;
        pixels.push_back(makeBGRPixel(192, 192, 192));

        fl::vector<u8> output;
        encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

        u8 expectedFlag = expectedFlagByte(192, 192, 192);
        CHECK_EQ(expectedFlag, 0xC0);
        CHECK_EQ(output[4], expectedFlag);
    }
}

TEST_CASE("P9813 - Multiple LEDs (three different colors)") {
    // Test encoding 3 LEDs with different RGB values
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(255, 0, 0));    // Red
    pixels.push_back(makeBGRPixel(0, 255, 0));    // Green
    pixels.push_back(makeBGRPixel(0, 0, 255));    // Blue

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 (start) + 3*4 (LEDs) + 4 (end) = 20 bytes
    REQUIRE_EQ(output.size(), 20);

    // Start boundary
    CHECK_EQ(output[0], 0x00);
    CHECK_EQ(output[1], 0x00);
    CHECK_EQ(output[2], 0x00);
    CHECK_EQ(output[3], 0x00);

    // LED 1: Red (255,0,0)
    CHECK_EQ(output[4], expectedFlagByte(255, 0, 0));
    CHECK_EQ(output[5], 0x00);   // Blue
    CHECK_EQ(output[6], 0x00);   // Green
    CHECK_EQ(output[7], 0xFF);   // Red

    // LED 2: Green (0,255,0)
    CHECK_EQ(output[8], expectedFlagByte(0, 255, 0));
    CHECK_EQ(output[9], 0x00);   // Blue
    CHECK_EQ(output[10], 0xFF);  // Green
    CHECK_EQ(output[11], 0x00);  // Red

    // LED 3: Blue (0,0,255)
    CHECK_EQ(output[12], expectedFlagByte(0, 0, 255));
    CHECK_EQ(output[13], 0xFF);  // Blue
    CHECK_EQ(output[14], 0x00);  // Green
    CHECK_EQ(output[15], 0x00);  // Red

    // End boundary
    CHECK_EQ(output[16], 0x00);
    CHECK_EQ(output[17], 0x00);
    CHECK_EQ(output[18], 0x00);
    CHECK_EQ(output[19], 0x00);
}

TEST_CASE("P9813 - Multiple LEDs (five LEDs with mixed values)") {
    // Test encoding 5 LEDs with various RGB combinations
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(255, 128, 64));   // Orange-ish
    pixels.push_back(makeBGRPixel(0, 0, 0));        // Black
    pixels.push_back(makeBGRPixel(255, 255, 255));  // White
    pixels.push_back(makeBGRPixel(100, 200, 50));   // Custom color
    pixels.push_back(makeBGRPixel(192, 64, 128));   // Another custom

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 (start) + 5*4 (LEDs) + 4 (end) = 28 bytes
    REQUIRE_EQ(output.size(), 28);

    // Verify start boundary
    for (int i = 0; i < 4; i++) {
        CHECK_EQ(output[i], 0x00);
    }

    // Verify each LED frame
    int offset = 4;

    // LED 1: (255, 128, 64) in BGR wire order: (64, 128, 255)
    CHECK_EQ(output[offset], expectedFlagByte(255, 128, 64));
    CHECK_EQ(output[offset + 1], 64);   // Blue
    CHECK_EQ(output[offset + 2], 128);  // Green
    CHECK_EQ(output[offset + 3], 255);  // Red
    offset += 4;

    // LED 2: (0, 0, 0)
    CHECK_EQ(output[offset], expectedFlagByte(0, 0, 0));
    CHECK_EQ(output[offset + 1], 0);    // Blue
    CHECK_EQ(output[offset + 2], 0);    // Green
    CHECK_EQ(output[offset + 3], 0);    // Red
    offset += 4;

    // LED 3: (255, 255, 255)
    CHECK_EQ(output[offset], expectedFlagByte(255, 255, 255));
    CHECK_EQ(output[offset + 1], 255);  // Blue
    CHECK_EQ(output[offset + 2], 255);  // Green
    CHECK_EQ(output[offset + 3], 255);  // Red
    offset += 4;

    // LED 4: (100, 200, 50) in BGR wire order: (50, 200, 100)
    CHECK_EQ(output[offset], expectedFlagByte(100, 200, 50));
    CHECK_EQ(output[offset + 1], 50);   // Blue
    CHECK_EQ(output[offset + 2], 200);  // Green
    CHECK_EQ(output[offset + 3], 100);  // Red
    offset += 4;

    // LED 5: (192, 64, 128) in BGR wire order: (128, 64, 192)
    CHECK_EQ(output[offset], expectedFlagByte(192, 64, 128));
    CHECK_EQ(output[offset + 1], 128);  // Blue
    CHECK_EQ(output[offset + 2], 64);   // Green
    CHECK_EQ(output[offset + 3], 192);  // Red
    offset += 4;

    // Verify end boundary
    for (int i = 24; i < 28; i++) {
        CHECK_EQ(output[i], 0x00);
    }
}

TEST_CASE("P9813 - Flag byte helper function verification") {
    // Direct tests of p9813FlagByte() helper function
    CHECK_EQ(p9813FlagByte(0, 0, 0), 0xFF);           // All black
    CHECK_EQ(p9813FlagByte(255, 255, 255), 0xC0);    // All white
    CHECK_EQ(p9813FlagByte(255, 0, 0), 0xFC);        // Pure red
    CHECK_EQ(p9813FlagByte(0, 255, 0), 0xF3);        // Pure green
    CHECK_EQ(p9813FlagByte(0, 0, 255), 0xCF);        // Pure blue
    CHECK_EQ(p9813FlagByte(64, 64, 64), 0xEA);       // Gray (01 pattern)
    CHECK_EQ(p9813FlagByte(128, 128, 128), 0xD5);    // Gray (10 pattern)
    CHECK_EQ(p9813FlagByte(192, 192, 192), 0xC0);    // Gray (11 pattern)

    // Edge cases with mixed bit patterns
    CHECK_EQ(p9813FlagByte(192, 0, 0), 0xFC);        // Red with high bits set
    CHECK_EQ(p9813FlagByte(0, 192, 0), 0xF3);        // Green with high bits set
    CHECK_EQ(p9813FlagByte(0, 0, 192), 0xCF);        // Blue with high bits set
}

TEST_CASE("P9813 - BGR wire order verification") {
    // Verify that pixel array is interpreted as BGR (not RGB)
    // Create a pixel where indices matter: B=100, G=150, R=200
    fl::array<u8, 3> pixel = {100, 150, 200};  // Wire order: BGR

    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(pixel);

    fl::vector<u8> output;
    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Flag byte should be calculated from RGB values: R=200, G=150, B=100
    u8 expectedFlag = expectedFlagByte(200, 150, 100);
    CHECK_EQ(output[4], expectedFlag);

    // Data bytes should be in BGR order as stored
    CHECK_EQ(output[5], 100);  // Blue (pixel[0])
    CHECK_EQ(output[6], 150);  // Green (pixel[1])
    CHECK_EQ(output[7], 200);  // Red (pixel[2])
}

TEST_CASE("P9813 - Output iterator compatibility") {
    // Test that encoder works with back_inserter (already tested above)
    // and verify it's compatible with generic output iterators
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makeBGRPixel(128, 128, 128));

    fl::vector<u8> output;
    output.reserve(12);  // Pre-allocate space

    encodeP9813(pixels.begin(), pixels.end(), fl::back_inserter(output));

    CHECK_EQ(output.size(), 12);
    CHECK_EQ(output[4], expectedFlagByte(128, 128, 128));
}

} // anonymous namespace
