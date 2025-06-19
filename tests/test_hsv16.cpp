// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/hsv16.h"
#include "fl/math.h"
#include "lib8tion/intmap.h"

using namespace fl;

TEST_CASE("RGB to HSV16 to RGB") {

    SUBCASE("Primary Colors - Good Conversion") {
        // Test colors that convert well with HSV16

        // Pure red - perfect conversion - expect exact match
        const int red_tolerance = 0; // Reduced from 1 to 0 - try for perfect
        CRGB red(255, 0, 0);
        HSV16 hsv_red(red);
        CRGB red_result = hsv_red.ToRGB();
        CHECK_CLOSE(red_result.r, red.r, red_tolerance);
        CHECK_CLOSE(red_result.g, red.g, red_tolerance);
        CHECK_CLOSE(red_result.b, red.b, red_tolerance);

        // Pure green - try for even better accuracy
        const int green_tolerance = 0; // Reduced from 2 to 0 - try for perfect!
        CRGB green(0, 255, 0);
        HSV16 hsv_green(green);
        CRGB green_result = hsv_green.ToRGB();
        CHECK_CLOSE(green_result.r, green.r, green_tolerance);
        CHECK_CLOSE(green_result.g, green.g, green_tolerance);
        CHECK_CLOSE(green_result.b, green.b, green_tolerance);

        // Pure blue - try for even better accuracy
        const int blue_tolerance = 0; // Reduced from 2 to 0 - try for perfect!
        CRGB blue(0, 0, 255);
        HSV16 hsv_blue(blue);
        CRGB blue_result = hsv_blue.ToRGB();
        CHECK_CLOSE(blue_result.r, blue.r, blue_tolerance);
        CHECK_CLOSE(blue_result.g, blue.g, blue_tolerance);
        CHECK_CLOSE(blue_result.b, blue.b, blue_tolerance);

        // Test black - perfect conversion expected
        CRGB black(0, 0, 0);
        HSV16 hsv_black(black);
        CRGB black_result = hsv_black.ToRGB();
        CHECK(black_result.r == black.r);
        CHECK(black_result.g == black.g);
        CHECK(black_result.b == black.b);
    }

    SUBCASE("White and Grayscale - Good Conversion") {
        // Test white - try for perfect accuracy
        const int white_tolerance = 0; // Reduced from 1 to 0 - try for perfect!
        CRGB white(255, 255, 255);
        HSV16 hsv_white(white);
        CRGB white_result = hsv_white.ToRGB();
        CHECK_CLOSE(white_result.r, white.r, white_tolerance);
        CHECK_CLOSE(white_result.g, white.g, white_tolerance);
        CHECK_CLOSE(white_result.b, white.b, white_tolerance);

        // Test various shades of gray - some require tolerance 1
        const int gray_tolerance =
            1; // Gray128 and Gray200 are off by exactly 1

        CRGB gray50(50, 50, 50);
        HSV16 hsv_gray50(gray50);
        CRGB gray50_result = hsv_gray50.ToRGB();
        CHECK_CLOSE(gray50_result.r, gray50.r,
                    0); // Gray50 is perfect - use tolerance 0
        CHECK_CLOSE(gray50_result.g, gray50.g, 0);
        CHECK_CLOSE(gray50_result.b, gray50.b, 0);

        CRGB gray128(128, 128, 128);
        HSV16 hsv_gray128(gray128);
        CRGB gray128_result = hsv_gray128.ToRGB();
        CHECK_CLOSE(gray128_result.r, gray128.r,
                    gray_tolerance); // Gray128 needs tolerance 1
        CHECK_CLOSE(gray128_result.g, gray128.g, gray_tolerance);
        CHECK_CLOSE(gray128_result.b, gray128.b, gray_tolerance);

        CRGB gray200(200, 200, 200);
        HSV16 hsv_gray200(gray200);
        CRGB gray200_result = hsv_gray200.ToRGB();
        CHECK_CLOSE(gray200_result.r, gray200.r,
                    gray_tolerance); // Gray200 needs tolerance 1
        CHECK_CLOSE(gray200_result.g, gray200.g, gray_tolerance);
        CHECK_CLOSE(gray200_result.b, gray200.b, gray_tolerance);
    }

    SUBCASE("HSV16 Constructor Values") {
        // Test direct HSV16 construction with known values
        const int direct_construction_tolerance =
            0; // Reduced from 2 to 0 - try for perfect!

        HSV16 hsv_red_direct(0, 65535, 65535); // Red: H=0, S=max, V=max
        CRGB red_direct_result = hsv_red_direct.ToRGB();
        CHECK(red_direct_result.r >=
              255 - direct_construction_tolerance); // Should be close to 255
        CHECK(red_direct_result.g <=
              direct_construction_tolerance); // Should be close to 0
        CHECK(red_direct_result.b <=
              direct_construction_tolerance); // Should be close to 0

        HSV16 hsv_green_direct(21845, 65535,
                               65535); // Green: H=1/3*65535, S=max, V=max
        CRGB green_direct_result = hsv_green_direct.ToRGB();
        CHECK(green_direct_result.r <=
              direct_construction_tolerance); // Should be close to 0
        CHECK(green_direct_result.g >=
              255 - direct_construction_tolerance); // Should be close to 255
        CHECK(green_direct_result.b <=
              direct_construction_tolerance); // Should be close to 0

        HSV16 hsv_blue_direct(43690, 65535,
                              65535); // Blue: H=2/3*65535, S=max, V=max
        CRGB blue_direct_result = hsv_blue_direct.ToRGB();
        CHECK(blue_direct_result.r <=
              direct_construction_tolerance); // Should be close to 0
        CHECK(blue_direct_result.g <=
              direct_construction_tolerance); // Should be close to 0
        CHECK(blue_direct_result.b >=
              255 - direct_construction_tolerance); // Should be close to 255

        // Test zero saturation (should produce grayscale)
        const int grayscale_direct_tolerance = 0; // Keep at 0 - already perfect
        HSV16 hsv_gray_direct(32768, 0,
                              32768); // Any hue, no saturation, half value
        CRGB gray_direct_result = hsv_gray_direct.ToRGB();
        CHECK_CLOSE(gray_direct_result.r, gray_direct_result.g,
                    grayscale_direct_tolerance);
        CHECK_CLOSE(gray_direct_result.g, gray_direct_result.b,
                    grayscale_direct_tolerance);
        CHECK(gray_direct_result.r >=
              128 - 1); // Reduced from 2 to 1 - even tighter
        CHECK(gray_direct_result.r <= 128 + 1);
    }

    SUBCASE("Secondary Colors - Good Conversion") {
        // These secondary colors should now convert accurately with the fixed
        // HSV16 implementation

        // Yellow should preserve both red and green components
        const int yellow_tolerance =
            0; // Reduced from 1 to 0 - perfect conversion!
        CRGB yellow(255, 255, 0);
        HSV16 hsv_yellow(yellow);
        CRGB yellow_result = hsv_yellow.ToRGB();
        CHECK_CLOSE(yellow_result.r, yellow.r,
                    yellow_tolerance); // Red should be preserved
        CHECK_CLOSE(yellow_result.g, yellow.g,
                    yellow_tolerance); // Green should be preserved
        CHECK_CLOSE(yellow_result.b, yellow.b,
                    yellow_tolerance); // Blue should stay 0

        // Cyan should preserve both green and blue components
        const int cyan_tolerance =
            0; // Reduced from 1 to 0 - perfect conversion!
        CRGB cyan(0, 255, 255);
        HSV16 hsv_cyan(cyan);
        CRGB cyan_result = hsv_cyan.ToRGB();
        CHECK_CLOSE(cyan_result.r, cyan.r, cyan_tolerance); // Red should stay 0
        CHECK_CLOSE(cyan_result.g, cyan.g,
                    cyan_tolerance); // Green should be preserved
        CHECK_CLOSE(cyan_result.b, cyan.b,
                    cyan_tolerance); // Blue should be preserved

        // Magenta should preserve both red and blue components
        const int magenta_tolerance =
            0; // Reduced from 1 to 0 - perfect conversion!
        CRGB magenta(255, 0, 255);
        HSV16 hsv_magenta(magenta);
        CRGB magenta_result = hsv_magenta.ToRGB();
        CHECK_CLOSE(magenta_result.r, magenta.r,
                    magenta_tolerance); // Red should be preserved
        CHECK_CLOSE(magenta_result.g, magenta.g,
                    magenta_tolerance); // Green should stay 0
        CHECK_CLOSE(magenta_result.b, magenta.b,
                    magenta_tolerance); // Blue should be preserved
    }

    SUBCASE("Low-Value Problematic Colors") {
        // Test very dark colors that are known to be problematic for HSV
        // conversion These colors often reveal quantization and rounding issues

        // Very dark red - near black but not black
        const int dark_primary_tolerance = 0; // Try for perfect conversion
        CRGB dark_red(10, 0, 0);
        HSV16 hsv_dark_red(dark_red);
        CRGB dark_red_result = hsv_dark_red.ToRGB();
        CHECK_CLOSE(dark_red_result.r, dark_red.r, dark_primary_tolerance);
        CHECK_CLOSE(dark_red_result.g, dark_red.g, dark_primary_tolerance);
        CHECK_CLOSE(dark_red_result.b, dark_red.b, dark_primary_tolerance);

        // Very dark green
        CRGB dark_green(0, 10, 0);
        HSV16 hsv_dark_green(dark_green);
        CRGB dark_green_result = hsv_dark_green.ToRGB();
        CHECK_CLOSE(dark_green_result.r, dark_green.r, dark_primary_tolerance);
        CHECK_CLOSE(dark_green_result.g, dark_green.g, dark_primary_tolerance);
        CHECK_CLOSE(dark_green_result.b, dark_green.b, dark_primary_tolerance);

        // Very dark blue
        CRGB dark_blue(0, 0, 10);
        HSV16 hsv_dark_blue(dark_blue);
        CRGB dark_blue_result = hsv_dark_blue.ToRGB();
        CHECK_CLOSE(dark_blue_result.r, dark_blue.r, dark_primary_tolerance);
        CHECK_CLOSE(dark_blue_result.g, dark_blue.g, dark_primary_tolerance);
        CHECK_CLOSE(dark_blue_result.b, dark_blue.b, dark_primary_tolerance);

        // Barely visible gray - single digit values
        const int barely_visible_tolerance = 0; // Try for perfect conversion
        CRGB barely_gray1(1, 1, 1);
        HSV16 hsv_barely_gray1(barely_gray1);
        CRGB barely_gray1_result = hsv_barely_gray1.ToRGB();
        CHECK_CLOSE(barely_gray1_result.r, barely_gray1.r,
                    barely_visible_tolerance);
        CHECK_CLOSE(barely_gray1_result.g, barely_gray1.g,
                    barely_visible_tolerance);
        CHECK_CLOSE(barely_gray1_result.b, barely_gray1.b,
                    barely_visible_tolerance);

        CRGB barely_gray5(5, 5, 5);
        HSV16 hsv_barely_gray5(barely_gray5);
        CRGB barely_gray5_result = hsv_barely_gray5.ToRGB();
        CHECK_CLOSE(barely_gray5_result.r, barely_gray5.r,
                    barely_visible_tolerance);
        CHECK_CLOSE(barely_gray5_result.g, barely_gray5.g,
                    barely_visible_tolerance);
        CHECK_CLOSE(barely_gray5_result.b, barely_gray5.b,
                    barely_visible_tolerance);

        // Low saturation, low value - muddy browns/grays
        const int muddy_tolerance = 1; // These may need tolerance 1
        CRGB muddy_brown(15, 10, 8);
        HSV16 hsv_muddy_brown(muddy_brown);
        CRGB muddy_brown_result = hsv_muddy_brown.ToRGB();
        CHECK_CLOSE(muddy_brown_result.r, muddy_brown.r, muddy_tolerance);
        CHECK_CLOSE(muddy_brown_result.g, muddy_brown.g, muddy_tolerance);
        CHECK_CLOSE(muddy_brown_result.b, muddy_brown.b, muddy_tolerance);

        // Edge case: slightly unequal very dark values
        CRGB dark_unequal(3, 2, 1);
        HSV16 hsv_dark_unequal(dark_unequal);
        CRGB dark_unequal_result = hsv_dark_unequal.ToRGB();
        CHECK_CLOSE(dark_unequal_result.r, dark_unequal.r, muddy_tolerance);
        CHECK_CLOSE(dark_unequal_result.g, dark_unequal.g, muddy_tolerance);
        CHECK_CLOSE(dark_unequal_result.b, dark_unequal.b, muddy_tolerance);

        // Very dark but colorful - low value, high saturation
        CRGB dark_saturated_red(20, 1, 1);
        HSV16 hsv_dark_saturated_red(dark_saturated_red);
        CRGB dark_saturated_red_result = hsv_dark_saturated_red.ToRGB();
        CHECK_CLOSE(dark_saturated_red_result.r, dark_saturated_red.r,
                    dark_primary_tolerance);
        CHECK_CLOSE(dark_saturated_red_result.g, dark_saturated_red.g,
                    dark_primary_tolerance);
        CHECK_CLOSE(dark_saturated_red_result.b, dark_saturated_red.b,
                    dark_primary_tolerance);
    }
}

TEST_CASE("Exhaustive round trip") {
    const int step = 4;
    for (int r = 0; r < 256; r+=step) {
        for (int g = 0; g < 256; g+=step) {
            for (int b = 0; b < 256; b+=step) {
                CRGB rgb(r, g, b);
                HSV16 hsv(rgb);
                CRGB rgb_result = hsv.ToRGB();
                REQUIRE_CLOSE(rgb_result.r, rgb.r, 1);
                REQUIRE_CLOSE(rgb_result.g, rgb.g, 1);
                REQUIRE_CLOSE(rgb_result.b, rgb.b, 1);
            }
        }
    }
}


#define TEST_VIDEO_RGB_HUE_PRESERVATION(color, hue_tolerance) \
    do { \
        HSV16 hsv_original(color); \
        uint16_t original_hue = hsv_original.h; \
        \
        CRGB video_result = hsv_original.colorBoost(); \
        HSV16 hsv_video_result(video_result); \
        uint16_t result_hue = hsv_video_result.h; \
        /* Special handling for hue around 0 (red) - check for wraparound */ \
        uint16_t hue_diff = (original_hue > result_hue) \
                                ? (original_hue - result_hue) \
                                : (result_hue - original_hue); \
        /* Also check wraparound case (difference near 65535) */ \
        uint16_t hue_diff_wraparound = 65535 - hue_diff; \
        uint16_t min_hue_diff = \
            (hue_diff < hue_diff_wraparound) ? hue_diff : hue_diff_wraparound; \
        \
        uint8_t hue_diff_8bit = map16_to_8(min_hue_diff); \
        \
        CHECK_LE(hue_diff_8bit, hue_tolerance); \
    } while(0)

TEST_CASE("colorBoost() preserves hue - easy cases") {

    // Helper function to test colorBoost() hue preservation

    // Test that colorBoost() preserves the hue while applying gamma
    // correction to saturation. Each color uses a fine-grained tolerance based
    // on empirically observed maximum hue differences.

    SUBCASE("Orange - Low hue error") {
        // Test with a vibrant orange color - wraparound helped reduce tolerance
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 128, 0), 0);
    }

    SUBCASE("Blue-Green - Moderate hue error") {
        // Test with a blue-green color - exactly 14 units max error observed
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(0, 200, 150), 0);
    }

    SUBCASE("Purple - Very low hue error") {
        // Test with a purple color - exactly 4 units max error observed
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(180, 50, 200), 0);
    }

    SUBCASE("Warm Yellow - Highest hue error case") {
        // Test with a warm yellow color - this is the worst case with exactly
        // 47 units max error (empirically determined as the absolute worst case
        // across all test colors)
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 220, 80), 0);
    }

    SUBCASE("Bright Red - Wraparound case") {
        // Test edge case: Very saturated red (hue around 0) - handle wraparound
        // Special case due to hue wraparound at 0/65535 boundary
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 30, 30), 0);
    }
}

TEST_CASE("colorBoost() preserves hue - hard cases") {

    SUBCASE("Low Saturation Colors - Hue Instability") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(130, 128, 125), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(100, 98, 102), 3);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(85, 87, 83), 0);
    }

    SUBCASE("Very Dark Colors - Low Value Instability") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(15, 10, 8), 1);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(12, 8, 20), 1);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(8, 15, 12), 1);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(20, 12, 8), 1);
    }

    SUBCASE("Hue Boundary Colors - Transition Regions") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 64, 0), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(192, 255, 0), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(0, 255, 128), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(0, 128, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(128, 0, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 0, 128), 0);
    }

    SUBCASE("Medium Saturation, Medium Value - Gamma Sensitive") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(180, 120, 60), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(120, 180, 90), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(90, 120, 180), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(180, 90, 150), 0);
    }

    SUBCASE("Single Component Dominant - Extreme Ratios") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(250, 10, 5), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(8, 240, 12), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(15, 8, 245), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 200, 8), 0);
    }

    SUBCASE("Pastel Colors - High Value, Low Saturation") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 200, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 255, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 200, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 255, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 200, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 255, 255), 0);
    }

    SUBCASE("Problematic RGB Combinations - Known Difficult Cases") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(77, 150, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 150, 77), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(150, 77, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(33, 66, 99), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(99, 33, 66), 0);
    }
}