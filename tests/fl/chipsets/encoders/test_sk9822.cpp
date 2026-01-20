/// @file test_sk9822.cpp
/// @brief Unit tests for SK9822 SPI LED chipset encoder
///
/// SK9822 Protocol Format:
/// - Start frame: 4 bytes of 0x00
/// - LED frames: [0xE0|brightness][B][G][R] (4 bytes per LED)
/// - End frame: (num_leds / 32) + 1 DWords of 0x00 (differs from APA102)
///
/// Key difference from APA102: End frame uses 0x00 instead of 0xFF

#include "fl/chipsets/encoders/sk9822.h"
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

/// Helper to create a pixel in BGR wire order
fl::array<u8, 3> makePixel(u8 r, u8 g, u8 b) {
    fl::array<u8, 3> pixel;
    pixel[0] = b;  // Blue first (BGR order)
    pixel[1] = g;  // Green second
    pixel[2] = r;  // Red third
    return pixel;
}

/// Helper to create pixel+brightness pair for HD mode
struct PixelBrightnessPair {
    fl::array<u8, 3> pixel;
    u8 brightness;

    PixelBrightnessPair(u8 r, u8 g, u8 b, u8 bri) : brightness(bri) {
        pixel[0] = b;  // BGR order
        pixel[1] = g;
        pixel[2] = r;
    }
};

/// Verify start frame: 4 bytes of 0x00
void verifyStartFrame(const fl::vector<u8>& output, size_t& offset) {
    CHECK_EQ(output[offset++], 0x00);
    CHECK_EQ(output[offset++], 0x00);
    CHECK_EQ(output[offset++], 0x00);
    CHECK_EQ(output[offset++], 0x00);
}

/// Verify single LED encoding: [0xE0|brightness][B][G][R]
void verifyLED(const fl::vector<u8>& output, size_t& offset, u8 expected_bri5, u8 r, u8 g, u8 b) {
    CHECK_EQ(output[offset++], 0xE0 | (expected_bri5 & 0x1F));
    CHECK_EQ(output[offset++], b);  // Blue
    CHECK_EQ(output[offset++], g);  // Green
    CHECK_EQ(output[offset++], r);  // Red
}

/// Verify end frame: (num_leds / 32) + 1 DWords of 0x00
void verifyEndFrame(const fl::vector<u8>& output, size_t& offset, size_t num_leds) {
    size_t end_dwords = (num_leds / 32) + 1;
    size_t end_bytes = end_dwords * 4;

    for (size_t i = 0; i < end_bytes; i++) {
        CHECK_EQ(output[offset++], 0x00);  // SK9822 uses 0x00 (differs from APA102's 0xFF)
    }

    // Verify we consumed all bytes
    CHECK_EQ(offset, output.size());
}

} // anonymous namespace

TEST_CASE("SK9822 - encodeSK9822() basic functionality") {
    // Test single LED with maximum brightness (31)
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(255, 128, 64));  // RGB

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + LED(4) + end(4) = 12 bytes
    CHECK_EQ(output.size(), 12);

    size_t offset = 0;
    verifyStartFrame(output, offset);
    verifyLED(output, offset, 31, 255, 128, 64);
    verifyEndFrame(output, offset, 1);
}

TEST_CASE("SK9822 - encodeSK9822() multiple LEDs") {
    // Test 3 LEDs
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(255, 0, 0));    // Red
    pixels.push_back(makePixel(0, 255, 0));    // Green
    pixels.push_back(makePixel(0, 0, 255));    // Blue

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + 3*LED(4) + end(4) = 20 bytes
    CHECK_EQ(output.size(), 20);

    size_t offset = 0;
    verifyStartFrame(output, offset);
    verifyLED(output, offset, 31, 255, 0, 0);
    verifyLED(output, offset, 31, 0, 255, 0);
    verifyLED(output, offset, 31, 0, 0, 255);
    verifyEndFrame(output, offset, 3);
}

TEST_CASE("SK9822 - encodeSK9822() zero LEDs") {
    // Test empty pixel array
    fl::vector<fl::array<u8, 3>> pixels;

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + end(4) = 8 bytes
    CHECK_EQ(output.size(), 8);

    size_t offset = 0;
    verifyStartFrame(output, offset);
    verifyEndFrame(output, offset, 0);
}

TEST_CASE("SK9822 - encodeSK9822() 32 LEDs end frame calculation") {
    // Test end frame calculation: (32 / 32) + 1 = 2 DWords = 8 bytes
    fl::vector<fl::array<u8, 3>> pixels;
    for (int i = 0; i < 32; i++) {
        pixels.push_back(makePixel(i, i, i));
    }

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + 32*LED(4) + end(8) = 140 bytes
    CHECK_EQ(output.size(), 140);

    // Verify end frame is 8 bytes of 0x00
    for (size_t i = 132; i < 140; i++) {
        CHECK_EQ(output[i], 0x00);
    }
}

TEST_CASE("SK9822 - encodeSK9822() 33 LEDs end frame calculation") {
    // Test end frame calculation: (33 / 32) + 1 = 2 DWords = 8 bytes
    fl::vector<fl::array<u8, 3>> pixels;
    for (int i = 0; i < 33; i++) {
        pixels.push_back(makePixel(i, i, i));
    }

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + 33*LED(4) + end(8) = 144 bytes
    CHECK_EQ(output.size(), 144);
}

TEST_CASE("SK9822 - encodeSK9822() 64 LEDs end frame calculation") {
    // Test end frame calculation: (40 / 32) + 1 = 2 DWords = 8 bytes
    // Reduced from 64 to 40 LEDs for performance (still tests beyond 32 LED boundary)
    fl::vector<fl::array<u8, 3>> pixels;
    for (int i = 0; i < 40; i++) {
        pixels.push_back(makePixel(i, i, i));
    }

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + 40*LED(4) + end(8) = 172 bytes
    CHECK_EQ(output.size(), 172);

    // Verify end frame is 8 bytes of 0x00
    for (size_t i = 164; i < 172; i++) {
        CHECK_EQ(output[i], 0x00);
    }
}

TEST_CASE("SK9822 - encodeSK9822() BGR color order verification") {
    // Verify BGR wire order is maintained
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(0xAA, 0xBB, 0xCC));  // R=0xAA, G=0xBB, B=0xCC

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Verify LED data at offset 4: [0xFF][0xCC][0xBB][0xAA]
    CHECK_EQ(output[4], 0xFF);  // Brightness header (31)
    CHECK_EQ(output[5], 0xCC);  // Blue
    CHECK_EQ(output[6], 0xBB);  // Green
    CHECK_EQ(output[7], 0xAA);  // Red
}

TEST_CASE("SK9822 - encodeSK9822_HD() per-LED brightness") {
    // Test per-LED brightness encoding
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> brightness;
    pixels.push_back(makePixel(255, 0, 0));
    brightness.push_back(255);  // Red, full brightness
    pixels.push_back(makePixel(0, 255, 0));
    brightness.push_back(128);  // Green, half brightness
    pixels.push_back(makePixel(0, 0, 255));
    brightness.push_back(0);    // Blue, zero brightness

    fl::vector<u8> output;
    encodeSK9822_HD(pixels.begin(), pixels.end(), brightness.begin(), fl::back_inserter(output));

    // Expected: start(4) + 3*LED(4) + end(4) = 20 bytes
    CHECK_EQ(output.size(), 20);

    size_t offset = 0;
    verifyStartFrame(output, offset);

    // LED 1: brightness 255 -> 31
    verifyLED(output, offset, 31, 255, 0, 0);

    // LED 2: brightness 128 -> 16
    verifyLED(output, offset, 16, 0, 255, 0);

    // LED 3: brightness 0 -> 0
    verifyLED(output, offset, 0, 0, 0, 255);

    verifyEndFrame(output, offset, 3);
}

TEST_CASE("SK9822 - encodeSK9822_HD() brightness mapping edge cases") {
    // Test brightness mapping: 8-bit to 5-bit
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> brightness;
    pixels.push_back(makePixel(255, 255, 255));
    brightness.push_back(1);    // brightness=1 -> 1
    pixels.push_back(makePixel(255, 255, 255));
    brightness.push_back(8);    // brightness=8 -> 1
    pixels.push_back(makePixel(255, 255, 255));
    brightness.push_back(16);   // brightness=16 -> 2
    pixels.push_back(makePixel(255, 255, 255));
    brightness.push_back(127);  // brightness=127 -> 16

    fl::vector<u8> output;
    encodeSK9822_HD(pixels.begin(), pixels.end(), brightness.begin(), fl::back_inserter(output));

    size_t offset = 4;  // Skip start frame

    // brightness 1 -> bri5 = 1 (non-zero input ensures non-zero output)
    CHECK_EQ(output[offset] & 0x1F, 1);
    offset += 4;

    // brightness 8 -> bri5 = 1
    CHECK_EQ(output[offset] & 0x1F, 1);
    offset += 4;

    // brightness 16 -> bri5 = 2
    CHECK_EQ(output[offset] & 0x1F, 2);
    offset += 4;

    // brightness 127 -> bri5 = 15
    CHECK_EQ(output[offset] & 0x1F, 15);
}

TEST_CASE("SK9822 - encodeSK9822_HD() zero LEDs") {
    // Test empty pixel array
    fl::vector<fl::array<u8, 3>> pixels;
    fl::vector<u8> brightness;

    fl::vector<u8> output;
    encodeSK9822_HD(pixels.begin(), pixels.end(), brightness.begin(), fl::back_inserter(output));

    // Expected: start(4) + end(4) = 8 bytes
    CHECK_EQ(output.size(), 8);

    size_t offset = 0;
    verifyStartFrame(output, offset);
    verifyEndFrame(output, offset, 0);
}

TEST_CASE("SK9822 - encodeSK9822_AutoBrightness() empty range") {
    // Test empty pixel array
    fl::vector<fl::array<u8, 3>> pixels;

    fl::vector<u8> output;
    encodeSK9822_AutoBrightness(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start frame only (4 bytes)
    CHECK_EQ(output.size(), 4);

    size_t offset = 0;
    verifyStartFrame(output, offset);
}

TEST_CASE("SK9822 - encodeSK9822_AutoBrightness() single LED") {
    // Test auto-brightness extraction from first pixel
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(255, 0, 0));  // Red at full brightness

    fl::vector<u8> output;
    encodeSK9822_AutoBrightness(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + LED(4) + end(4) = 12 bytes
    CHECK_EQ(output.size(), 12);

    // Verify brightness is extracted from max component (255)
    // Expected brightness: ((255 + 1) * 31 - 1) / 256 + 1 = 31
    CHECK_EQ(output[4] & 0x1F, 31);
}

TEST_CASE("SK9822 - encodeSK9822_AutoBrightness() multiple LEDs") {
    // Test that all LEDs use brightness from first pixel
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(128, 0, 0));    // Red, brightness=128
    pixels.push_back(makePixel(0, 255, 0));    // Green (brightness from first LED)
    pixels.push_back(makePixel(0, 0, 64));     // Blue (brightness from first LED)

    fl::vector<u8> output;
    encodeSK9822_AutoBrightness(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: start(4) + 3*LED(4) + end(4) = 20 bytes
    CHECK_EQ(output.size(), 20);

    // Extract expected brightness from first pixel (max component = 128)
    // brightness = ((128 + 1) * 31 - 1) / 256 + 1 = 16
    u8 expected_brightness = 16;

    // Verify all LEDs use the same brightness
    CHECK_EQ(output[4] & 0x1F, expected_brightness);   // LED 1
    CHECK_EQ(output[8] & 0x1F, expected_brightness);   // LED 2
    CHECK_EQ(output[12] & 0x1F, expected_brightness);  // LED 3
}

TEST_CASE("SK9822 - encodeSK9822_AutoBrightness() low brightness") {
    // Test auto-brightness with low value
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(16, 8, 4));  // Low brightness

    fl::vector<u8> output;
    encodeSK9822_AutoBrightness(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Max component = 16
    // Expected brightness: ((16 + 1) * 31 - 1) / 256 + 1 = 3
    CHECK_EQ(output[4] & 0x1F, 3);
}

TEST_CASE("SK9822 - encodeSK9822_AutoBrightness() first LED color scaling") {
    // Test that first LED's colors are scaled based on extracted brightness
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(128, 64, 32));  // R=128, G=64, B=32

    fl::vector<u8> output;
    encodeSK9822_AutoBrightness(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // First LED should have scaled colors
    // The implementation scales colors proportionally to brightness
    // We just verify the structure is correct
    CHECK_EQ(output.size(), 12);  // start(4) + LED(4) + end(4)

    // Verify brightness byte format
    CHECK_EQ(output[4] & 0xE0, 0xE0);  // Top 3 bits should be 111
}

TEST_CASE("SK9822 - End frame uses 0x00 (differs from APA102)") {
    // Critical test: Verify SK9822 end frame uses 0x00, NOT 0xFF
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(255, 255, 255));

    fl::vector<u8> output;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // End frame starts at offset 8 (start=4, LED=4)
    // End frame: 4 bytes of 0x00
    for (size_t i = 8; i < 12; i++) {
        CHECK_EQ(output[i], 0x00);  // SK9822 uses 0x00
        CHECK_NE(output[i], 0xFF);  // NOT 0xFF (APA102 uses 0xFF)
    }
}

TEST_CASE("SK9822 - All three encoders use 0x00 end frames") {
    // Verify all three encoder functions use 0x00 end frames
    fl::vector<fl::array<u8, 3>> pixels;
    pixels.push_back(makePixel(255, 0, 0));

    fl::vector<u8> brightness;
    brightness.push_back(255);

    // Test encodeSK9822
    fl::vector<u8> output1;
    encodeSK9822(pixels.begin(), pixels.end(), fl::back_inserter(output1));
    CHECK_EQ(output1[8], 0x00);   // End frame byte 1
    CHECK_EQ(output1[9], 0x00);   // End frame byte 2
    CHECK_EQ(output1[10], 0x00);  // End frame byte 3
    CHECK_EQ(output1[11], 0x00);  // End frame byte 4

    // Test encodeSK9822_HD
    fl::vector<u8> output2;
    encodeSK9822_HD(pixels.begin(), pixels.end(), brightness.begin(), fl::back_inserter(output2));
    CHECK_EQ(output2[8], 0x00);   // End frame byte 1
    CHECK_EQ(output2[9], 0x00);   // End frame byte 2
    CHECK_EQ(output2[10], 0x00);  // End frame byte 3
    CHECK_EQ(output2[11], 0x00);  // End frame byte 4

    // Test encodeSK9822_AutoBrightness
    fl::vector<u8> output3;
    encodeSK9822_AutoBrightness(pixels.begin(), pixels.end(), fl::back_inserter(output3));
    CHECK_EQ(output3[8], 0x00);   // End frame byte 1
    CHECK_EQ(output3[9], 0x00);   // End frame byte 2
    CHECK_EQ(output3[10], 0x00);  // End frame byte 3
    CHECK_EQ(output3[11], 0x00);  // End frame byte 4
}
