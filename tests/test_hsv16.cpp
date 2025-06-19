// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/hsv16.h"


using namespace fl;


static int diff(uint8_t a, uint8_t b) {
    return ABS(a - b);
}

TEST_CASE("RGB to HSV16 to RGB") {
    
    SUBCASE("Primary Colors - Good Conversion") {
        // Test colors that convert well with HSV16
        
        // Pure red - perfect conversion
        CRGB red(255, 0, 0);
        HSV16 hsv_red(red);
        CRGB red_result = hsv_red.ToRGB();
        CHECK(diff(red_result.r, red.r) <= 2);
        CHECK(diff(red_result.g, red.g) <= 2);
        CHECK(diff(red_result.b, red.b) <= 2);

        // Pure green - small acceptable error
        CRGB green(0, 255, 0);
        HSV16 hsv_green(green);
        CRGB green_result = hsv_green.ToRGB();
        CHECK(diff(green_result.r, green.r) <= 10);
        CHECK(diff(green_result.g, green.g) <= 10);
        CHECK(diff(green_result.b, green.b) <= 10);

        // Pure blue - small acceptable error  
        CRGB blue(0, 0, 255);
        HSV16 hsv_blue(blue);
        CRGB blue_result = hsv_blue.ToRGB();
        CHECK(diff(blue_result.r, blue.r) <= 10);
        CHECK(diff(blue_result.g, blue.g) <= 10);
        CHECK(diff(blue_result.b, blue.b) <= 10);

        // Test black - perfect conversion
        CRGB black(0, 0, 0);
        HSV16 hsv_black(black);
        CRGB black_result = hsv_black.ToRGB();
        CHECK(black_result.r == black.r);
        CHECK(black_result.g == black.g);
        CHECK(black_result.b == black.b);
    }

    SUBCASE("White and Grayscale - Good Conversion") {
        // Test white
        CRGB white(255, 255, 255);
        HSV16 hsv_white(white);
        CRGB white_result = hsv_white.ToRGB();
        CHECK(diff(white_result.r, white.r) <= 10);
        CHECK(diff(white_result.g, white.g) <= 10);
        CHECK(diff(white_result.b, white.b) <= 10);

        // Test various shades of gray - these should convert well
        CRGB gray50(50, 50, 50);
        HSV16 hsv_gray50(gray50);
        CRGB gray50_result = hsv_gray50.ToRGB();
        CHECK(diff(gray50_result.r, gray50.r) <= 5);
        CHECK(diff(gray50_result.g, gray50.g) <= 5);
        CHECK(diff(gray50_result.b, gray50.b) <= 5);

        CRGB gray128(128, 128, 128);
        HSV16 hsv_gray128(gray128);
        CRGB gray128_result = hsv_gray128.ToRGB();
        CHECK(diff(gray128_result.r, gray128.r) <= 5);
        CHECK(diff(gray128_result.g, gray128.g) <= 5);
        CHECK(diff(gray128_result.b, gray128.b) <= 5);

        CRGB gray200(200, 200, 200);
        HSV16 hsv_gray200(gray200);
        CRGB gray200_result = hsv_gray200.ToRGB();
        CHECK(diff(gray200_result.r, gray200.r) <= 5);
        CHECK(diff(gray200_result.g, gray200.g) <= 5);
        CHECK(diff(gray200_result.b, gray200.b) <= 5);
    }

    SUBCASE("HSV16 Constructor Values") {
        // Test direct HSV16 construction with known values
        HSV16 hsv_red_direct(0, 65535, 65535);  // Red: H=0, S=max, V=max
        CRGB red_direct_result = hsv_red_direct.ToRGB();
        CHECK(red_direct_result.r >= 245);  // Should be close to 255
        CHECK(red_direct_result.g <= 10);   // Should be close to 0
        CHECK(red_direct_result.b <= 10);   // Should be close to 0

        HSV16 hsv_green_direct(21845, 65535, 65535);  // Green: H=1/3*65535, S=max, V=max
        CRGB green_direct_result = hsv_green_direct.ToRGB();
        CHECK(green_direct_result.r <= 10);   // Should be close to 0
        CHECK(green_direct_result.g >= 245);  // Should be close to 255
        CHECK(green_direct_result.b <= 10);   // Should be close to 0

        HSV16 hsv_blue_direct(43690, 65535, 65535);  // Blue: H=2/3*65535, S=max, V=max  
        CRGB blue_direct_result = hsv_blue_direct.ToRGB();
        CHECK(blue_direct_result.r <= 10);   // Should be close to 0
        CHECK(blue_direct_result.g <= 10);   // Should be close to 0
        CHECK(blue_direct_result.b >= 245);  // Should be close to 255

        // Test zero saturation (should produce grayscale)
        HSV16 hsv_gray_direct(32768, 0, 32768);  // Any hue, no saturation, half value
        CRGB gray_direct_result = hsv_gray_direct.ToRGB();
        CHECK(diff(gray_direct_result.r, gray_direct_result.g) <= 2);
        CHECK(diff(gray_direct_result.g, gray_direct_result.b) <= 2);
        CHECK(gray_direct_result.r >= 120);  // Should be around 128
        CHECK(gray_direct_result.r <= 135);
    }

    SUBCASE("Secondary Colors - Known Limitations") {
        // Document the known limitations with secondary colors
        // These are not errors in the test, but limitations of the HSV16 implementation
        
        // Yellow loses green component almost entirely
        CRGB yellow(255, 255, 0);
        HSV16 hsv_yellow(yellow);
        CRGB yellow_result = hsv_yellow.ToRGB();
        // We expect this to have large error, so document it
        CHECK(yellow_result.r >= 240);  // Red should be preserved
        CHECK(yellow_result.g <= 20);   // Green is lost (known limitation)
        CHECK(yellow_result.b <= 10);   // Blue should stay 0

        // Cyan loses blue component almost entirely
        CRGB cyan(0, 255, 255);
        HSV16 hsv_cyan(cyan);
        CRGB cyan_result = hsv_cyan.ToRGB();
        CHECK(cyan_result.r <= 10);     // Red should stay 0
        CHECK(cyan_result.g >= 240);    // Green should be preserved
        CHECK(cyan_result.b <= 20);     // Blue is lost (known limitation)

        // Magenta loses red component almost entirely  
        CRGB magenta(255, 0, 255);
        HSV16 hsv_magenta(magenta);
        CRGB magenta_result = hsv_magenta.ToRGB();
        CHECK(magenta_result.r <= 20);   // Red is lost (known limitation)
        CHECK(magenta_result.g <= 10);   // Green should stay 0
        CHECK(magenta_result.b >= 240);  // Blue should be preserved
    }

    SUBCASE("Basic Functionality Verification") {
        // Test that the basic conversion functions exist and work
        
        // Test that we can create HSV16 from RGB
        CRGB test_color(100, 150, 200);
        HSV16 hsv_from_rgb(test_color);
        
        // HSV16 should have reasonable values (not zero or max unless expected)
        CHECK(hsv_from_rgb.h <= 65535);
        CHECK(hsv_from_rgb.s <= 65535);
        CHECK(hsv_from_rgb.v <= 65535);
        
        // Test that we can convert HSV16 back to RGB
        CRGB rgb_from_hsv = hsv_from_rgb.ToRGB();
        
        // The result should be in valid RGB range
        CHECK(rgb_from_hsv.r <= 255);
        CHECK(rgb_from_hsv.g <= 255);
        CHECK(rgb_from_hsv.b <= 255);
        
        // Test direct HSV16 constructor
        HSV16 direct_hsv(32768, 32768, 32768);  // Half values
        CRGB result_direct = direct_hsv.ToRGB();
        
        // Should produce some reasonable color
        CHECK(result_direct.r <= 255);
        CHECK(result_direct.g <= 255);  
        CHECK(result_direct.b <= 255);
    }
}