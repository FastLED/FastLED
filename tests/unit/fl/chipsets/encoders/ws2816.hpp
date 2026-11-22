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
#include "test.h"
#include "fl/stl/int.h"
#include "fl/gfx/crgb.h"
#include "fl/stl/array.h"
#include "fl/stl/vector.h"
#include "fl/stl/pair.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/gfx/pixel_iterator_any.h"
#include "pixel_controller.h"

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
    FL_CHECK_EQ(pixel1.r, expected_r_hi);
    FL_CHECK_EQ(pixel1.g, expected_r_lo);
    FL_CHECK_EQ(pixel1.b, expected_g_hi);

    // Second pixel: [G_lo, B_hi, B_lo]
    FL_CHECK_EQ(pixel2.r, expected_g_lo);
    FL_CHECK_EQ(pixel2.g, expected_b_hi);
    FL_CHECK_EQ(pixel2.b, expected_b_lo);
}

using namespace test_ws2816;

// ============================================================================
// packWS2816Pixel() Tests - Helper Function Verification
// ============================================================================

FL_TEST_CASE("packWS2816Pixel - all zeros") {
    // Test packing (0, 0, 0) → two black CRGB pixels
    auto packed = packWS2816Pixel(0x0000, 0x0000, 0x0000);

    FL_CHECK_EQ(packed.first.r, 0x00);
    FL_CHECK_EQ(packed.first.g, 0x00);
    FL_CHECK_EQ(packed.first.b, 0x00);
    FL_CHECK_EQ(packed.second.r, 0x00);
    FL_CHECK_EQ(packed.second.g, 0x00);
    FL_CHECK_EQ(packed.second.b, 0x00);
}

FL_TEST_CASE("packWS2816Pixel - all max values") {
    // Test packing (0xFFFF, 0xFFFF, 0xFFFF) → two white CRGB pixels
    auto packed = packWS2816Pixel(0xFFFF, 0xFFFF, 0xFFFF);

    FL_CHECK_EQ(packed.first.r, 0xFF);
    FL_CHECK_EQ(packed.first.g, 0xFF);
    FL_CHECK_EQ(packed.first.b, 0xFF);
    FL_CHECK_EQ(packed.second.r, 0xFF);
    FL_CHECK_EQ(packed.second.g, 0xFF);
    FL_CHECK_EQ(packed.second.b, 0xFF);
}

FL_TEST_CASE("packWS2816Pixel - red channel only (high byte)") {
    // Test R = 0xFF00 (high byte only), G = 0, B = 0
    // Expected: [0xFF, 0x00, 0x00] and [0x00, 0x00, 0x00]
    auto packed = packWS2816Pixel(0xFF00, 0x0000, 0x0000);

    FL_CHECK_EQ(packed.first.r, 0xFF);   // R_hi
    FL_CHECK_EQ(packed.first.g, 0x00);   // R_lo
    FL_CHECK_EQ(packed.first.b, 0x00);   // G_hi
    FL_CHECK_EQ(packed.second.r, 0x00);  // G_lo
    FL_CHECK_EQ(packed.second.g, 0x00);  // B_hi
    FL_CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

FL_TEST_CASE("packWS2816Pixel - red channel only (low byte)") {
    // Test R = 0x00FF (low byte only), G = 0, B = 0
    // Expected: [0x00, 0xFF, 0x00] and [0x00, 0x00, 0x00]
    auto packed = packWS2816Pixel(0x00FF, 0x0000, 0x0000);

    FL_CHECK_EQ(packed.first.r, 0x00);   // R_hi
    FL_CHECK_EQ(packed.first.g, 0xFF);   // R_lo
    FL_CHECK_EQ(packed.first.b, 0x00);   // G_hi
    FL_CHECK_EQ(packed.second.r, 0x00);  // G_lo
    FL_CHECK_EQ(packed.second.g, 0x00);  // B_hi
    FL_CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

FL_TEST_CASE("packWS2816Pixel - green channel split") {
    // Test R = 0, G = 0xAABB (split across both pixels), B = 0
    // Expected: [0x00, 0x00, 0xAA] and [0xBB, 0x00, 0x00]
    auto packed = packWS2816Pixel(0x0000, 0xAABB, 0x0000);

    FL_CHECK_EQ(packed.first.r, 0x00);   // R_hi
    FL_CHECK_EQ(packed.first.g, 0x00);   // R_lo
    FL_CHECK_EQ(packed.first.b, 0xAA);   // G_hi
    FL_CHECK_EQ(packed.second.r, 0xBB);  // G_lo ← split point!
    FL_CHECK_EQ(packed.second.g, 0x00);  // B_hi
    FL_CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

FL_TEST_CASE("packWS2816Pixel - blue channel only (high byte)") {
    // Test R = 0, G = 0, B = 0xFF00 (high byte only)
    // Expected: [0x00, 0x00, 0x00] and [0x00, 0xFF, 0x00]
    auto packed = packWS2816Pixel(0x0000, 0x0000, 0xFF00);

    FL_CHECK_EQ(packed.first.r, 0x00);   // R_hi
    FL_CHECK_EQ(packed.first.g, 0x00);   // R_lo
    FL_CHECK_EQ(packed.first.b, 0x00);   // G_hi
    FL_CHECK_EQ(packed.second.r, 0x00);  // G_lo
    FL_CHECK_EQ(packed.second.g, 0xFF);  // B_hi
    FL_CHECK_EQ(packed.second.b, 0x00);  // B_lo
}

FL_TEST_CASE("packWS2816Pixel - blue channel only (low byte)") {
    // Test R = 0, G = 0, B = 0x00FF (low byte only)
    // Expected: [0x00, 0x00, 0x00] and [0x00, 0x00, 0xFF]
    auto packed = packWS2816Pixel(0x0000, 0x0000, 0x00FF);

    FL_CHECK_EQ(packed.first.r, 0x00);   // R_hi
    FL_CHECK_EQ(packed.first.g, 0x00);   // R_lo
    FL_CHECK_EQ(packed.first.b, 0x00);   // G_hi
    FL_CHECK_EQ(packed.second.r, 0x00);  // G_lo
    FL_CHECK_EQ(packed.second.g, 0x00);  // B_hi
    FL_CHECK_EQ(packed.second.b, 0xFF);  // B_lo
}

FL_TEST_CASE("packWS2816Pixel - mixed values") {
    // Test R = 0x1234, G = 0x5678, B = 0x9ABC
    // Expected: [0x12, 0x34, 0x56] and [0x78, 0x9A, 0xBC]
    auto packed = packWS2816Pixel(0x1234, 0x5678, 0x9ABC);

    FL_CHECK_EQ(packed.first.r, 0x12);   // R_hi
    FL_CHECK_EQ(packed.first.g, 0x34);   // R_lo
    FL_CHECK_EQ(packed.first.b, 0x56);   // G_hi
    FL_CHECK_EQ(packed.second.r, 0x78);  // G_lo
    FL_CHECK_EQ(packed.second.g, 0x9A);  // B_hi
    FL_CHECK_EQ(packed.second.b, 0xBC);  // B_lo
}

FL_TEST_CASE("packWS2816Pixel - sequential pattern") {
    // Test sequential hex values: R = 0x0102, G = 0x0304, B = 0x0506
    // Expected: [0x01, 0x02, 0x03] and [0x04, 0x05, 0x06]
    auto packed = packWS2816Pixel(0x0102, 0x0304, 0x0506);

    FL_CHECK_EQ(packed.first.r, 0x01);
    FL_CHECK_EQ(packed.first.g, 0x02);
    FL_CHECK_EQ(packed.first.b, 0x03);
    FL_CHECK_EQ(packed.second.r, 0x04);
    FL_CHECK_EQ(packed.second.g, 0x05);
    FL_CHECK_EQ(packed.second.b, 0x06);
}

// ============================================================================
// encodeWS2816() Tests - Full Encoder Verification
// ============================================================================

FL_TEST_CASE("encodeWS2816 - empty range (0 LEDs)") {
    // Test encoding with no LEDs - should produce no output
    fl::vector<fl::array<u16, 3>> pixels;
    fl::vector<CRGB> output;

    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 0 CRGB pixels (no frame overhead for WS2816)
    FL_REQUIRE_EQ(output.size(), 0);
}

FL_TEST_CASE("encodeWS2816 - single pixel (all zeros)") {
    // Test single black pixel (0, 0, 0)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x0000, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 2 CRGB pixels (1 input → 2 output)
    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
}

FL_TEST_CASE("encodeWS2816 - single pixel (all max)") {
    // Test single white pixel (0xFFFF, 0xFFFF, 0xFFFF)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xFFFF, 0xFFFF, 0xFFFF));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 2 CRGB pixels
    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
}

FL_TEST_CASE("encodeWS2816 - single pixel (red high byte)") {
    // Test R = 0xFF00, G = 0, B = 0
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xFF00, 0x0000, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00);
}

FL_TEST_CASE("encodeWS2816 - single pixel (red low byte)") {
    // Test R = 0x00FF, G = 0, B = 0
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x00FF, 0x0000, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00);
}

FL_TEST_CASE("encodeWS2816 - single pixel (green split)") {
    // Test R = 0, G = 0xAABB (split across pixels), B = 0
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0xAABB, 0x0000));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0xAA, 0xBB, 0x00, 0x00);
}

FL_TEST_CASE("encodeWS2816 - single pixel (blue high byte)") {
    // Test R = 0, G = 0, B = 0xFF00
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x0000, 0xFF00));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00);
}

FL_TEST_CASE("encodeWS2816 - single pixel (blue low byte)") {
    // Test R = 0, G = 0, B = 0x00FF
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x0000, 0x00FF));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF);
}

FL_TEST_CASE("encodeWS2816 - single pixel (mixed values)") {
    // Test R = 0x1234, G = 0x5678, B = 0x9ABC
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x1234, 0x5678, 0x9ABC));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);
}

FL_TEST_CASE("encodeWS2816 - multiple pixels (2 LEDs)") {
    // Test 2 pixels: (0x1122, 0x3344, 0x5566) and (0x7788, 0x99AA, 0xBBCC)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x1122, 0x3344, 0x5566));
    pixels.push_back(makePixel16(0x7788, 0x99AA, 0xBBCC));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 4 CRGB pixels (2 input → 4 output)
    FL_REQUIRE_EQ(output.size(), 4);

    // First input pixel
    verifyDualPixel(output[0], output[1], 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);

    // Second input pixel
    verifyDualPixel(output[2], output[3], 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC);
}

FL_TEST_CASE("encodeWS2816 - multiple pixels (3 LEDs)") {
    // Test 3 distinct pixels
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xFF00, 0x0000, 0x0000));  // Red high
    pixels.push_back(makePixel16(0x0000, 0xFF00, 0x0000));  // Green high
    pixels.push_back(makePixel16(0x0000, 0x0000, 0xFF00));  // Blue high

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 6 CRGB pixels (3 input → 6 output)
    FL_REQUIRE_EQ(output.size(), 6);

    verifyDualPixel(output[0], output[1], 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00);
    verifyDualPixel(output[2], output[3], 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00);
    verifyDualPixel(output[4], output[5], 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00);
}

FL_TEST_CASE("encodeWS2816 - boundary values (min/max per channel)") {
    // Test extreme values: min (0x0000), mid (0x8000), max (0xFFFF)
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0000, 0x8000, 0xFFFF));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    verifyDualPixel(output[0], output[1], 0x00, 0x00, 0x80, 0x00, 0xFF, 0xFF);
}

FL_TEST_CASE("encodeWS2816 - sequential hex pattern") {
    // Test sequential hex values for debugging
    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0x0102, 0x0304, 0x0506));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);
    FL_CHECK_EQ(output[0].r, 0x01);
    FL_CHECK_EQ(output[0].g, 0x02);
    FL_CHECK_EQ(output[0].b, 0x03);
    FL_CHECK_EQ(output[1].r, 0x04);
    FL_CHECK_EQ(output[1].g, 0x05);
    FL_CHECK_EQ(output[1].b, 0x06);
}

FL_TEST_CASE("encodeWS2816 - large array (30 pixels)") {
    // Test encoding large arrays efficiently (reduced from 100 to 30 for performance)
    // Still provides excellent coverage: 30 pixels = 60 CRGB output = adequate stress test
    fl::vector<fl::array<u16, 3>> pixels;
    for (int i = 0; i < 30; ++i) {
        pixels.push_back(makePixel16(i * 0x0101, i * 0x0202, i * 0x0303));
    }

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    // Expected: 60 CRGB pixels (30 input → 60 output)
    FL_REQUIRE_EQ(output.size(), 60);

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

FL_TEST_CASE("encodeWS2816 - channel layout documentation") {
    // This test documents the exact channel layout for WS2816 encoding
    // Input: R16 = 0xRrRr, G16 = 0xGgGg, B16 = 0xBbBb (uppercase = high, lowercase = low)
    // Output: CRGB1 = [Rr, Rr, Gg], CRGB2 = [Gg, Bb, Bb]

    fl::vector<fl::array<u16, 3>> pixels;
    pixels.push_back(makePixel16(0xABCD, 0xEF01, 0x2345));

    fl::vector<CRGB> output;
    encodeWS2816(pixels.begin(), pixels.end(), fl::back_inserter(output));

    FL_REQUIRE_EQ(output.size(), 2);

    // First CRGB: [R_hi=0xAB, R_lo=0xCD, G_hi=0xEF]
    FL_CHECK_EQ(output[0].r, 0xAB);
    FL_CHECK_EQ(output[0].g, 0xCD);
    FL_CHECK_EQ(output[0].b, 0xEF);

    // Second CRGB: [G_lo=0x01, B_hi=0x23, B_lo=0x45]
    FL_CHECK_EQ(output[1].r, 0x01);
    FL_CHECK_EQ(output[1].g, 0x23);
    FL_CHECK_EQ(output[1].b, 0x45);
}

} // namespace test_ws2816


//-----------------------------------------------------------------------------
// #4323: the rgb16 adapter must yield the pixel, not the correction constant
//
// Everything else in this file drives packWS2816Pixel/encodeWS2816 with plain
// fl::vector iterators, so nothing observed what a PixelIterator actually
// hands them. Under FASTLED_HD_COLOR_MIXING the adapter called
// loadRGBScaleAndBrightness() and treated its outputs as the pixel; those are
// ColorAdjustment::color, a per-strip constant, so every LED came out the same
// colour. WS2816 is the only user of this range.
//-----------------------------------------------------------------------------

FL_TEST_CASE("[#4323] the rgb16 adapter yields each pixel, not one flat colour") {
    fl::CRGB leds[3] = {fl::CRGB(255, 0, 0), fl::CRGB(0, 255, 0), fl::CRGB(0, 0, 255)};
    PixelController<RGB> source(leds, 3, ColorAdjustment::noAdjustment(),
                                DISABLE_DITHER);
    fl::PixelIteratorAny adapter(source, RGB, fl::Rgbw());

    fl::vector<fl::array<fl::u16, 3>> seen;
    auto range = fl::makeScaledPixelRangeRGB16(&adapter.get());
    for (auto it = range.first; it != range.second; ++it) {
        seen.push_back(*it);
    }

    FL_CHECK_EQ((int)seen.size(), 3);
    if (seen.size() == 3) {
        // Red, green, blue -- each in its own channel and nowhere else. The
        // bug returned (65535, 65535, 65535) three times, which is why the
        // off-channels are checked and not just the on-channel.
        FL_CHECK_EQ((int)seen[0][0], 65535);
        FL_CHECK_EQ((int)seen[0][1], 0);
        FL_CHECK_EQ((int)seen[0][2], 0);

        FL_CHECK_EQ((int)seen[1][0], 0);
        FL_CHECK_EQ((int)seen[1][1], 65535);
        FL_CHECK_EQ((int)seen[1][2], 0);

        FL_CHECK_EQ((int)seen[2][0], 0);
        FL_CHECK_EQ((int)seen[2][1], 0);
        FL_CHECK_EQ((int)seen[2][2], 65535);
    }
}

FL_TEST_CASE("[#4323] brightness reaches the rgb16 adapter exactly once") {
    // A vacuity guard on the case above: it uses noAdjustment(), where the
    // correction constant (255,255,255) and a full-brightness white pixel are
    // easy to confuse. Here brightness is 128, so a constant-returning adapter
    // and a correct one differ in value, not just in position.
    ColorAdjustment adj = ColorAdjustment::noAdjustment();
    adj.brightness = 128;
    adj.premixed = fl::CRGB(128, 128, 128);   // what PixelController computes
    adj.color = fl::CRGB(255, 255, 255);

    fl::CRGB leds[1] = {fl::CRGB(255, 0, 0)};
    PixelController<RGB> source(leds, 1, adj, DISABLE_DITHER);
    fl::PixelIteratorAny adapter(source, RGB, fl::Rgbw());

    auto range = fl::makeScaledPixelRangeRGB16(&adapter.get());
    const fl::array<fl::u16, 3> first = *range.first;

    // 255 scaled by 128/255 is 128, mapped to 16 bits. Applied twice it would
    // be 64 -> 16448; not at all, 65535.
    FL_CHECK_EQ((int)first[0], (int)fl::map8_to_16(128));
    FL_CHECK_EQ((int)first[1], 0);
    FL_CHECK_EQ((int)first[2], 0);
}

FL_TEST_CASE("[#4323] a WS2816 frame carries three different LEDs to the wire") {
    // End to end, the way WS2816Controller::showPixels does it: adapter into
    // encodeWS2816. Three LEDs of the same colour would have satisfied any
    // per-LED check, so the claim is that the encoded LEDs differ.
    fl::CRGB leds[3] = {fl::CRGB(255, 0, 0), fl::CRGB(0, 255, 0), fl::CRGB(0, 0, 255)};
    PixelController<RGB> source(leds, 3, ColorAdjustment::noAdjustment(),
                                DISABLE_DITHER);
    fl::PixelIteratorAny adapter(source, RGB, fl::Rgbw());

    fl::vector<CRGB> out;
    auto range = fl::makeScaledPixelRangeRGB16(&adapter.get());
    encodeWS2816(range.first, range.second, fl::back_inserter(out));

    // Two CRGBs per LED, three LEDs.
    FL_CHECK_EQ((int)out.size(), 6);
    if (out.size() == 6) {
        // LED 0 is red: [R_hi, R_lo, G_hi] = [0xFF, 0xFF, 0x00].
        FL_CHECK_EQ((int)out[0].r, 0xFF);
        FL_CHECK_EQ((int)out[0].g, 0xFF);
        FL_CHECK_EQ((int)out[0].b, 0x00);
        // LED 1 is green, so its red bytes are zero where LED 0's were full.
        FL_CHECK_EQ((int)out[2].r, 0x00);
        FL_CHECK_EQ((int)out[2].g, 0x00);
        FL_CHECK_EQ((int)out[2].b, 0xFF);
        // And the three LEDs are not all the same pair, which is the whole
        // failure this pins.
        const bool all_equal = (out[0] == out[2]) && (out[0] == out[4]);
        FL_CHECK_FALSE(all_equal);
    }
}
