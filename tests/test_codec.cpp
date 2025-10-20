#include "test.h"
#include "fl/codec/pixel.h"


TEST_CASE("RGB565 to RGB888 conversion validation") {
    // This test doesn't need filesystem setup
        MESSAGE("Validating RGB565 to RGB888 lookup tables against reference implementation");

        // Reference function using floating point arithmetic with proper rounding
        auto rgb565ToRgb888_reference = [](fl::u16 rgb565, fl::u8& r, fl::u8& g, fl::u8& b) {
            // Extract RGB components from RGB565
            fl::u8 r5 = (rgb565 >> 11) & 0x1F;  // 5-bit red
            fl::u8 g6 = (rgb565 >> 5) & 0x3F;   // 6-bit green
            fl::u8 b5 = rgb565 & 0x1F;          // 5-bit blue

            // Scale to full 8-bit range using floating point and rounding (mathematically optimal)
            r = (fl::u8)((r5 * 255.0) / 31.0 + 0.5);  // 5 bits: max=31, scale to 0-255 with rounding
            g = (fl::u8)((g6 * 255.0) / 63.0 + 0.5);  // 6 bits: max=63, scale to 0-255 with rounding
            b = (fl::u8)((b5 * 255.0) / 31.0 + 0.5);  // 5 bits: max=31, scale to 0-255 with rounding
        };

        // Test Red component progression (0,0,0) -> (255,0,0)
        MESSAGE("Testing Red component progression through all 32 possible values");
        for (int red5 = 0; red5 <= 31; red5++) {
            // Create RGB565 value with only red component
            fl::u16 rgb565 = red5 << 11;  // Red in bits 15-11

            // Test lookup table function
            fl::u8 r_lookup, g_lookup, b_lookup;
            fl::rgb565ToRgb888(rgb565, r_lookup, g_lookup, b_lookup);

            // Test reference function
            fl::u8 r_ref, g_ref, b_ref;
            rgb565ToRgb888_reference(rgb565, r_ref, g_ref, b_ref);

            // Validate lookup table matches reference
            CHECK_EQ(r_lookup, r_ref);
            CHECK_EQ(g_lookup, g_ref);  // Should be 0
            CHECK_EQ(b_lookup, b_ref);  // Should be 0

            // Validate expected values
            CHECK_EQ(g_lookup, 0);      // Green should be 0
            CHECK_EQ(b_lookup, 0);      // Blue should be 0
        }
        MESSAGE("✅ Red component: All 32 values validated against reference");

        // Test Green component progression (0,0,0) -> (0,255,0)
        MESSAGE("Testing Green component progression through all 64 possible values");
        for (int green6 = 0; green6 <= 63; green6++) {
            // Create RGB565 value with only green component
            fl::u16 rgb565 = green6 << 5;  // Green in bits 10-5

            // Test lookup table function
            fl::u8 r_lookup, g_lookup, b_lookup;
            fl::rgb565ToRgb888(rgb565, r_lookup, g_lookup, b_lookup);

            // Test reference function
            fl::u8 r_ref, g_ref, b_ref;
            rgb565ToRgb888_reference(rgb565, r_ref, g_ref, b_ref);

            // Validate lookup table matches reference
            CHECK_EQ(r_lookup, r_ref);  // Should be 0
            CHECK_EQ(g_lookup, g_ref);
            CHECK_EQ(b_lookup, b_ref);  // Should be 0

            // Validate expected values
            CHECK_EQ(r_lookup, 0);      // Red should be 0
            CHECK_EQ(b_lookup, 0);      // Blue should be 0
        }
        MESSAGE("✅ Green component: All 64 values validated against reference");

        // Test Blue component progression (0,0,0) -> (0,0,255)
        MESSAGE("Testing Blue component progression through all 32 possible values");
        for (int blue5 = 0; blue5 <= 31; blue5++) {
            // Create RGB565 value with only blue component
            fl::u16 rgb565 = blue5;  // Blue in bits 4-0

            // Test lookup table function
            fl::u8 r_lookup, g_lookup, b_lookup;
            fl::rgb565ToRgb888(rgb565, r_lookup, g_lookup, b_lookup);

            // Test reference function
            fl::u8 r_ref, g_ref, b_ref;
            rgb565ToRgb888_reference(rgb565, r_ref, g_ref, b_ref);

            // Validate lookup table matches reference
            CHECK_EQ(r_lookup, r_ref);  // Should be 0
            CHECK_EQ(g_lookup, g_ref);  // Should be 0
            CHECK_EQ(b_lookup, b_ref);

            // Validate expected values
            CHECK_EQ(r_lookup, 0);      // Red should be 0
            CHECK_EQ(g_lookup, 0);      // Green should be 0
        }
        MESSAGE("✅ Blue component: All 32 values validated against reference");

        // Test boundary conditions and random sampling
        fl::u8 r_lookup, g_lookup, b_lookup;
        fl::u8 r_ref, g_ref, b_ref;

        // Test minimum values (all 0)
        fl::rgb565ToRgb888(0x0000, r_lookup, g_lookup, b_lookup);
        rgb565ToRgb888_reference(0x0000, r_ref, g_ref, b_ref);
        CHECK_EQ(r_lookup, r_ref);
        CHECK_EQ(g_lookup, g_ref);
        CHECK_EQ(b_lookup, b_ref);
        CHECK_EQ(r_lookup, 0);
        CHECK_EQ(g_lookup, 0);
        CHECK_EQ(b_lookup, 0);

        // Test maximum values (all 1s)
        fl::rgb565ToRgb888(0xFFFF, r_lookup, g_lookup, b_lookup);
        rgb565ToRgb888_reference(0xFFFF, r_ref, g_ref, b_ref);
        CHECK_EQ(r_lookup, r_ref);
        CHECK_EQ(g_lookup, g_ref);
        CHECK_EQ(b_lookup, b_ref);
        CHECK_EQ(r_lookup, 255);
        CHECK_EQ(g_lookup, 255);
        CHECK_EQ(b_lookup, 255);

        // Test random RGB565 values
        fl::u16 test_values[] = {
            0x0000, 0x001F, 0x07E0, 0xF800, 0xFFFF,  // Pure colors
            0x1234, 0x5678, 0x9ABC, 0xCDEF,          // Random values
            0x7BEF, 0x39E7, 0xC618, 0x8410           // More random values
        };

        for (auto rgb565 : test_values) {
            // Test lookup table function
            fl::rgb565ToRgb888(rgb565, r_lookup, g_lookup, b_lookup);

            // Test reference function
            rgb565ToRgb888_reference(rgb565, r_ref, g_ref, b_ref);

            // Validate lookup table matches reference exactly
            CHECK_EQ(r_lookup, r_ref);
            CHECK_EQ(g_lookup, g_ref);
            CHECK_EQ(b_lookup, b_ref);
        }

    MESSAGE("✅ RGB565 to RGB888 conversion: All tests passed - lookup table validated against reference");
}

TEST_CASE("RGB565 specific color values test") {
        fl::u8 r, g, b;

        // Test pure red (RGB565: 0xF800 = 11111 000000 00000)
        fl::rgb565ToRgb888(0xF800, r, g, b);
        CHECK_EQ(r, 255);  // 5-bit 31 -> 8-bit 255
        CHECK_EQ(g, 0);    // 6-bit 0 -> 8-bit 0
        CHECK_EQ(b, 0);    // 5-bit 0 -> 8-bit 0

        // Test pure green (RGB565: 0x07E0 = 00000 111111 00000)
        fl::rgb565ToRgb888(0x07E0, r, g, b);
        CHECK_EQ(r, 0);    // 5-bit 0 -> 8-bit 0
        CHECK_EQ(g, 255);  // 6-bit 63 -> 8-bit 255
        CHECK_EQ(b, 0);    // 5-bit 0 -> 8-bit 0

        // Test pure blue (RGB565: 0x001F = 00000 000000 11111)
        fl::rgb565ToRgb888(0x001F, r, g, b);
        CHECK_EQ(r, 0);    // 5-bit 0 -> 8-bit 0
        CHECK_EQ(g, 0);    // 6-bit 0 -> 8-bit 0
        CHECK_EQ(b, 255);  // 5-bit 31 -> 8-bit 255

        // Test white (RGB565: 0xFFFF = 11111 111111 11111)
        fl::rgb565ToRgb888(0xFFFF, r, g, b);
        CHECK_EQ(r, 255);  // 5-bit 31 -> 8-bit 255
        CHECK_EQ(g, 255);  // 6-bit 63 -> 8-bit 255
        CHECK_EQ(b, 255);  // 5-bit 31 -> 8-bit 255

        // Test black (RGB565: 0x0000 = 00000 000000 00000)
        fl::rgb565ToRgb888(0x0000, r, g, b);
    CHECK_EQ(r, 0);    // 5-bit 0 -> 8-bit 0
    CHECK_EQ(g, 0);    // 6-bit 0 -> 8-bit 0
    CHECK_EQ(b, 0);    // 5-bit 0 -> 8-bit 0
}

TEST_CASE("RGB565 scaling accuracy test") {
        fl::u8 r, g, b;

        // Test mid-range values to verify proper scaling
        // RGB565: 0x7BEF = (15 << 11) | (47 << 5) | 15 = (15,47,15)
        // From lookup table: 15->123, 47->125, 15->123
        fl::rgb565ToRgb888(0x7BEF, r, g, b);

        // Verify values match lookup table exactly
        CHECK_EQ(r, 123);  // rgb565_5to8_table[15] = 123
        CHECK_EQ(g, 125);  // rgb565_6to8_table[47] = 125
        CHECK_EQ(b, 123);  // rgb565_5to8_table[15] = 123

    MESSAGE("Mid-range test - RGB565: 0x7BEF -> RGB888: (" << (int)r << "," << (int)g << "," << (int)b << ")");
}

TEST_CASE("RGB565 full range scaling test") {
        fl::u8 r, g, b;

        // Test that maximum 5-bit value (31) scales to 255
        fl::rgb565ToRgb888(0xF800, r, g, b);  // Red: 11111 000000 00000
        CHECK_EQ(r, 255);

        // Test that maximum 6-bit value (63) scales to 255
        fl::rgb565ToRgb888(0x07E0, r, g, b);  // Green: 00000 111111 00000
        CHECK_EQ(g, 255);

        // Test that minimum values scale to 0
        fl::rgb565ToRgb888(0x0000, r, g, b);
    CHECK_EQ(r, 0);
    CHECK_EQ(g, 0);
    CHECK_EQ(b, 0);
}

TEST_CASE("RGB565 intermediate values test") {
        fl::u8 r, g, b;

        // Test some intermediate values to ensure they're not 0 or 255
        // RGB565: 0x4208 (8,16,8)
        fl::u16 rgb565 = (8 << 11) | (16 << 5) | 8;
        fl::rgb565ToRgb888(rgb565, r, g, b);

        CHECK(r > 0);
        CHECK(r < 255);
        CHECK(g > 0);
        CHECK(g < 255);
        CHECK(b > 0);
        CHECK(b < 255);

    MESSAGE("Intermediate test - RGB565: " << rgb565
            << " -> RGB888: (" << (int)r << "," << (int)g << "," << (int)b << ")");
}
