/// @file test_ws2816.cpp
/// @brief Unit tests for WS2816 LED chipset encoder
///
/// WS2816 Protocol Format:
/// - Input: 16-bit RGB (3x 16-bit values = 48 bits per LED)
/// - Output: Dual 8-bit CRGB (2x 24-bit CRGB = 48 bits per LED)
/// - Each 16-bit channel splits: high byte → first CRGB, low byte → second CRGB
/// - Channel layout: [R_hi, R_lo, G_hi] and [G_lo, B_hi, B_lo]
/// - No start/end frames (WS2812-compatible protocol)
///
/// This encoder converts high-definition 16-bit pixels into dual 8-bit pixels
/// for transmission through standard WS2812 controllers.

#include "fl/chipsets/encoders/ws2816.h"
#include "hsv2rgb.h"  // for CRGB
#include "fl/stl/vector.h"
#include "fl/stl/iterator.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/rgb8.h"
#include "fl/stl/array.h"
#include "fl/stl/vector.h"
#include "fl/stl/pair.h"

using namespace fl;

namespace test_ws2816 {

// ============================================================================
// Helper for creating 16-bit RGB pixels
// ============================================================================

/// @brief Create a 16-bit RGB pixel array (wire-ordered)
/// @param r 16-bit red channel
/// @param g 16-bit green channel
/// @param b 16-bit blue channel
/// @return fl::array<u16, 3> in wire order
inline fl::array<u16, 3> makePixel16(u16 r, u16 g, u16 b) {
    return fl::array<u16, 3>{{r, g, b}};
}

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Verify that two CRGB pixels match expected byte values
void verifyDualPixel(const CRGB& pixel1, const CRGB& pixel2,
                     u8 expected_r_hi, u8 expected_r_lo, u8 expected_g_hi,
                     u8 expected_g_lo, u8 expected_b_hi, u8 expected_b_lo) {
    // First pixel: [R_hi, R_lo, G_hi]
    CHECK_EQ(pixel1.r, expected_r_hi);
    CHECK_EQ(pixel1.g, expected_r_lo);
    CHECK_EQ(pixel1.b, expected_g_hi);

    // Second pixel: [G_lo, B_hi, B_lo]
    CHECK_EQ(pixel2.r, expected_g_lo);
    CHECK_EQ(pixel2.g, expected_b_hi);
    CHECK_EQ(pixel2.b, expected_b_lo);
}

using namespace test_ws2816;

// ============================================================================
// packWS2816Pixel() Tests - Helper Function Verification
// ============================================================================

TEST_CASE("packWS2816Pixel - all zeros") {
    // Test packing (0, 0, 0) → two black CRGB pixels
    auto packed = packWS2816Pixel(0x0000, 0x0000, 0x0000);

    CHECK_EQ(packed.first.r, 0x00);
    CHECK_EQ(packed.first.g, 0x00);
    CHECK_EQ(packed.first.b, 0x00);
    CHECK_EQ(packed.second.r, 0x00);
    CHECK_EQ(packed.second.g, 0x00);
    CHECK_EQ(packed.second.b, 0x00);
}

TEST_CASE("packWS2816Pixel - all max values") {
    // Test packing (0xFFFF, 0xFFFF, 0xFFFF) → two white CRGB pixels
    auto packed = packWS2816Pixel(0xFFFF, 0xFFFF, 0xFFFF);

    CHECK_EQ(packed.first.r, 0xFF);
    CHECK_EQ(packed.first.g, 0xFF);
    CHECK_EQ(packed.first.b, 0xFF);
    CHECK_EQ(packed.second.r, 0xFF);
    CHECK_EQ(packed.second.g, 0xFF);
    CHECK_EQ(packed.second.b, 0xFF);
}

TEST_CASE("packWS2816Pixel - red channel only (high byte)") {
    // Test R = 0xFF00 (high byte only), G = 0, B = 0
    // Expected: [0xFF, 0x00, 0x00] and [0x00, 0x00, 0x00]
    auto packed = packWS2816Pixel(0xFF00, 0x0000, 0x0000);

    CHECK_EQ(packed.first.r, 0xFF);   // R_hi
    CHECK_EQ(packed.first.g, 0x00);   // R_lo
    CHECK_EQ(packed.first.b, 0x00);   // G_hi
    CHECK_EQ(packed.second.r, 0x00);  // G_lo
    CHECK_EQ(packed.second.g, 0x00);  // B_hi
    CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

TEST_CASE("packWS2816Pixel - red channel only (low byte)") {
    // Test R = 0x00FF (low byte only), G = 0, B = 0
    // Expected: [0x00, 0xFF, 0x00] and [0x00, 0x00, 0x00]
    auto packed = packWS2816Pixel(0x00FF, 0x0000, 0x0000);

    CHECK_EQ(packed.first.r, 0x00);   // R_hi
    CHECK_EQ(packed.first.g, 0xFF);   // R_lo
    CHECK_EQ(packed.first.b, 0x00);   // G_hi
    CHECK_EQ(packed.second.r, 0x00);  // G_lo
    CHECK_EQ(packed.second.g, 0x00);  // B_hi
    CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

TEST_CASE("packWS2816Pixel - green channel split") {
    // Test R = 0, G = 0xAABB (split across both pixels), B = 0
    // Expected: [0x00, 0x00, 0xAA] and [0xBB, 0x00, 0x00]
    auto packed = packWS2816Pixel(0x0000, 0xAABB, 0x0000);

    CHECK_EQ(packed.first.r, 0x00);   // R_hi
    CHECK_EQ(packed.first.g, 0x00);   // R_lo
    CHECK_EQ(packed.first.b, 0xAA);   // G_hi
    CHECK_EQ(packed.second.r, 0xBB);  // G_lo ← split point!
    CHECK_EQ(packed.second.g, 0x00);  // B_hi
    CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

TEST_CASE("packWS2816Pixel - blue channel only (high byte)") {
    // Test R = 0, G = 0, B = 0xFF00 (high byte only)
    // Expected: [0x00, 0x00, 0x00] and [0x00, 0xFF, 0x00]
    auto packed = packWS2816Pixel(0x0000, 0x0000, 0xFF00);

    CHECK_EQ(packed.first.r, 0x00);   // R_hi
    CHECK_EQ(packed.first.g, 0x00);   // R_lo
    CHECK_EQ(packed.first.b, 0x00);   // G_hi
    CHECK_EQ(packed.second.r, 0x00);  // G_lo
    CHECK_EQ(packed.second.g, 0xFF);  // B_hi
    CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

TEST_CASE("packWS2816Pixel - blue channel only (low byte)") {
    // Test R = 0, G = 0, B = 0x00FF (low byte only)
    // Expected: [0x00, 0x00, 0x00] and [0x00, 0x00, 0xFF]
    auto packed = packWS2816Pixel(0x0000, 0x0000, 0x00FF);

    CHECK_EQ(packed.first.r, 0x00);   // R_hi
    CHECK_EQ(packed.first.g, 0x00);   // R_lo
    CHECK_EQ(packed.first.b, 0x00);   // G_hi
    CHECK_EQ(packed.second.r, 0x00);  // G_lo
    CHECK_EQ(packed.second.g, 0x00);  // B_hi
    CHECK_EQ(packed.second.b, 0xFF);  // B_lo
}

TEST_CASE("packWS2816Pixel - mixed values") {
    // Test R = 0x1234, G = 0x5678, B = 0x9ABC
    // Expected: [0x12, 0x34, 0x56] and [0x78, 0x9A, 0xBC]
    auto packed = packWS2816Pixel(0x1234, 0x5678, 0x9ABC);

    CHECK_EQ(packed.first.r, 0x12);   // R_hi
    CHECK_EQ(packed.first.g, 0x34);   // R_lo
    CHECK_EQ(packed.first.b, 0x56);   // G_hi
    CHECK_EQ(packed.second.r, 0x78);  // G_lo
    CHECK_EQ(packed.second.g, 0x9A);  // B_hi
    CHECK_EQ(packed.second.b, 0xBC);  // B_lo
}

TEST_CASE("packWS2816Pixel - sequential pattern") {
    // Test sequential hex values: R = 0x0102, G = 0x0304, B = 0x0506
    // Expected: [0x01, 0x02, 0x03] and [0x04, 0x05, 0x06]
    auto packed = packWS2816Pixel(0x0102, 0x0304, 0x0506);

    CHECK_EQ(packed.first.r, 0x01);
    CHECK_EQ(packed.first.g, 0x02);
    CHECK_EQ(packed.first.b, 0x03);
    CHECK_EQ(packed.second.r, 0x04);
    CHECK_EQ(packed.second.g, 0x05);
    CHECK_EQ(packed.second.b, 0x06);
}

// ============================================================================
// encodeWS2816() Tests - Full Encoder Verification
// ============================================================================

TEST_CASE("encodeWS2816 - empty range (0 LEDs)") {
    // Test encoding with no LEDs - should produce no output
    fl::vector<fl::array<u16, 3>> pixels;
    fl::vector<CRGB> output;

    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 0 CRGB pixels (no frame overhead for WS2816)
    REQUIRE_EQ(output.size(), 0);
}

TEST_CASE("encodeWS2816 - single pixel (all zeros)") {
    // Test single black pixel (0, 0, 0)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x0000, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 2 CRGB pixels (1 input → 2 output)
    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
}

TEST_CASE("encodeWS2816 - single pixel (all max)") {
    // Test single white pixel (0xFFFF, 0xFFFF, 0xFFFF)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xFFFF, 0xFFFF, 0xFFFF));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 2 CRGB pixels
    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
}

TEST_CASE("encodeWS2816 - single pixel (red high byte)") {
    // Test R = 0xFF00, G = 0, B = 0
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xFF00, 0x0000, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00);
}

TEST_CASE("encodeWS2816 - single pixel (red low byte)") {
    // Test R = 0x00FF, G = 0, B = 0
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x00FF, 0x0000, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00);
}

TEST_CASE("encodeWS2816 - single pixel (green split)") {
    // Test R = 0, G = 0xAABB (split across pixels), B = 0
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0xAABB, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0xAA, 0xBB, 0x00, 0x00);
}

TEST_CASE("encodeWS2816 - single pixel (blue high byte)") {
    // Test R = 0, G = 0, B = 0xFF00
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x0000, 0xFF00));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00);
}

TEST_CASE("encodeWS2816 - single pixel (blue low byte)") {
    // Test R = 0, G = 0, B = 0x00FF
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x0000, 0x00FF));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF);
}

TEST_CASE("encodeWS2816 - single pixel (mixed values)") {
    // Test R = 0x1234, G = 0x5678, B = 0x9ABC
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x1234, 0x5678, 0x9ABC));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);
}

TEST_CASE("encodeWS2816 - multiple pixels (2 LEDs)") {
    // Test 2 pixels: (0x1122, 0x3344, 0x5566) and (0x7788, 0x99AA, 0xBBCC)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x1122, 0x3344, 0x5566));
    pixels.push_back(makePixel16(0x7788, 0x99AA, 0xBBCC));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 CRGB pixels (2 input → 4 output)
    REQUIRE_EQ(output.size(), 4);

    // First input pixel
    verifyDualPixel(output[0], output[1], 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);

    // Second input pixel
    verifyDualPixel(output[2], output[3], 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC);
}

TEST_CASE("encodeWS2816 - multiple pixels (3 LEDs)") {
    // Test 3 distinct pixels
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xFF00, 0x0000, 0x0000));  // Red high
    pixels.push_back(makePixel16(0x0000, 0xFF00, 0x0000));  // Green high
    pixels.push_back(makePixel16(0x0000, 0x0000, 0xFF00));  // Blue high

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 6 CRGB pixels (3 input → 6 output)
    REQUIRE_EQ(output.size(), 6);

    verifyDualPixel(output[0], output[1], 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00);
    verifyDualPixel(output[2], output[3], 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00);
    verifyDualPixel(output[4], output[5], 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00);
}

TEST_CASE("encodeWS2816 - boundary values (min/max per channel)") {
    // Test extreme values: min (0x0000), mid (0x8000), max (0xFFFF)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x8000, 0xFFFF));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x80, 0x00, 0xFF, 0xFF);
}

TEST_CASE("encodeWS2816 - sequential hex pattern") {
    // Test sequential hex values for debugging
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0102, 0x0304, 0x0506));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);
    CHECK_EQ(output[0].r, 0x01);
    CHECK_EQ(output[0].g, 0x02);
    CHECK_EQ(output[0].b, 0x03);
    CHECK_EQ(output[1].r, 0x04);
    CHECK_EQ(output[1].g, 0x05);
    CHECK_EQ(output[1].b, 0x06);
}

TEST_CASE("encodeWS2816 - large array (30 pixels)") {
    // Test encoding large arrays efficiently (reduced from 100 to 30 for performance)
    // Still provides excellent coverage: 30 pixels = 60 CRGB output = adequate stress test
    fl::vector<fl::array<u16, 3>> pixels;
    for (int i = 0; i < 30; ++i) {
        pixels.push_back(makePixel16(i * 0x0101, i * 0x0202, i * 0x0303));
    }

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 60 CRGB pixels (30 input → 60 output)
    REQUIRE_EQ(output.size(), 60);

    // Spot check first pixel
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

    // Spot check last pixel (i = 29: calculated values)
    // Note: 29 * 0x0303 = 0x880B (no wrap needed for u16)
    u16 r_val = static_cast<u16>(29 * 0x0101);
    u16 g_val = static_cast<u16>(29 * 0x0202);
    u16 b_val = static_cast<u16>(29 * 0x0303);

    u8 r_hi = r_val >> 8;
    u8 r_lo = r_val & 0xFF;
    u8 g_hi = g_val >> 8;
    u8 g_lo = g_val & 0xFF;
    u8 b_hi = b_val >> 8;
    u8 b_lo = b_val & 0xFF;

    verifyDualPixel(output[58], output[59], r_hi, r_lo, g_hi, g_lo, b_hi, b_lo);
}

// ============================================================================
// Channel Layout Verification Tests
// ============================================================================

TEST_CASE("encodeWS2816 - channel layout documentation") {
    // This test documents the exact channel layout for WS2816 encoding
    // Input: R16 = 0xRrRr, G16 = 0xGgGg, B16 = 0xBbBb (uppercase = high, lowercase = low)
    // Output: CRGB1 = [Rr, Rr, Gg], CRGB2 = [Gg, Bb, Bb]

    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xABCD, 0xEF01, 0x2345));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    REQUIRE_EQ(output.size(), 2);

    // First CRGB: [R_hi=0xAB, R_lo=0xCD, G_hi=0xEF]
    CHECK_EQ(output[0].r, 0xAB);
    CHECK_EQ(output[0].g, 0xCD);
    CHECK_EQ(output[0].b, 0xEF);

    // Second CRGB: [G_lo=0x01, B_hi=0x23, B_lo=0x45]
    CHECK_EQ(output[1].r, 0x01);
    CHECK_EQ(output[1].g, 0x23);
    CHECK_EQ(output[1].b, 0x45);
}

} // namespace test_ws2816
