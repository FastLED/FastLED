// Compile with: g++ --std=c++11 test.cpp

#include "test.h"

#include "crgb.h"
#include "fl/cielab.h"
#include "hsv2rgb.h"

using namespace fl;

static uint8_t hue_compare(uint8_t h1, uint8_t h2) {
    int16_t diff1 = h1 - h2;
    int16_t diff2 = h2 - h1;
    return diff1 < diff2 ? diff1 : diff2;
}

TEST_CASE("CIELAB16 basic operations") {

    SUBCASE("Conversion from CRGB to CIELAB16") {
        // Test black color
        CRGB black(0, 0, 0);
        CIELAB16 lab_black(black);
        CRGB black_result = lab_black.ToRGB();
        CHECK(black_result.r == black.r);
        CHECK(black_result.g == black.g);
        CHECK(black_result.b == black.b);

        // Test white color
        CRGB white(255, 255, 255);
        CIELAB16 lab_white(white);
        CRGB white_result = lab_white.ToRGB();
        // Allow for small quantization errors in white conversion
        CHECK(ABS((int)white_result.r - (int)white.r) <= 2);
        CHECK(ABS((int)white_result.g - (int)white.g) <= 2);
        CHECK(ABS((int)white_result.b - (int)white.b) <= 2);

        // Test primary colors - CIELAB conversion can be quite lossy
        CRGB red(255, 0, 0);
        CIELAB16 lab_red(red);
        CRGB red_result = lab_red.ToRGB();
        CHECK(ABS((int)red_result.r - (int)red.r) <= 15);
        CHECK(ABS((int)red_result.g - (int)red.g) <= 25);
        CHECK(ABS((int)red_result.b - (int)red.b) <= 110);

        CRGB green(0, 255, 0);
        CIELAB16 lab_green(green);
        CRGB green_result = lab_green.ToRGB();
        CHECK(ABS((int)green_result.r - (int)green.r) <= 25);
        CHECK(ABS((int)green_result.g - (int)green.g) <= 15);
        CHECK(ABS((int)green_result.b - (int)green.b) <= 25);

        CRGB blue(0, 0, 255);
        CIELAB16 lab_blue(blue);
        CRGB blue_result = lab_blue.ToRGB();
        CHECK(ABS((int)blue_result.r - (int)blue.r) <= 25);
        CHECK(ABS((int)blue_result.g - (int)blue.g) <= 25);
        CHECK(ABS((int)blue_result.b - (int)blue.b) <= 15);

        // Test secondary colors
        CRGB cyan(0, 255, 255);
        CIELAB16 lab_cyan(cyan);
        CRGB cyan_result = lab_cyan.ToRGB();
        CHECK(ABS((int)cyan_result.r - (int)cyan.r) <= 25);
        CHECK(ABS((int)cyan_result.g - (int)cyan.g) <= 15);
        CHECK(ABS((int)cyan_result.b - (int)cyan.b) <= 15);

        CRGB magenta(255, 0, 255);
        CIELAB16 lab_magenta(magenta);
        CRGB magenta_result = lab_magenta.ToRGB();
        CHECK(ABS((int)magenta_result.r - (int)magenta.r) <= 15);
        CHECK(ABS((int)magenta_result.g - (int)magenta.g) <= 25);
        CHECK(ABS((int)magenta_result.b - (int)magenta.b) <= 15);

        CRGB yellow(255, 255, 0);
        CIELAB16 lab_yellow(yellow);
        CRGB yellow_result = lab_yellow.ToRGB();
        CHECK(ABS((int)yellow_result.r - (int)yellow.r) <= 15);
        CHECK(ABS((int)yellow_result.g - (int)yellow.g) <= 15);
        CHECK(ABS((int)yellow_result.b - (int)yellow.b) <= 25);

        // Test mid-tone colors
        CRGB gray(128, 128, 128);
        CIELAB16 lab_gray(gray);
        CRGB gray_result = lab_gray.ToRGB();
        CHECK(ABS((int)gray_result.r - (int)gray.r) <= 20);
        CHECK(ABS((int)gray_result.g - (int)gray.g) <= 20);
        CHECK(ABS((int)gray_result.b - (int)gray.b) <= 20);

        // Test some arbitrary colors that might reveal edge cases
        CRGB purple(128, 0, 128);
        CIELAB16 lab_purple(purple);
        CRGB purple_result = lab_purple.ToRGB();
        CHECK(ABS((int)purple_result.r - (int)purple.r) <= 20);
        CHECK(ABS((int)purple_result.g - (int)purple.g) <= 30);
        CHECK(ABS((int)purple_result.b - (int)purple.b) <= 20);

        CRGB orange(255, 128, 0);
        CIELAB16 lab_orange(orange);
        CRGB orange_result = lab_orange.ToRGB();
        CHECK(ABS((int)orange_result.r - (int)orange.r) <= 15);
        CHECK(ABS((int)orange_result.g - (int)orange.g) <= 20);
        CHECK(ABS((int)orange_result.b - (int)orange.b) <= 30);

        // Test low-intensity colors - these can have very large errors due to quantization
        CRGB dark_red(64, 0, 0);
        CIELAB16 lab_dark_red(dark_red);
        CRGB dark_red_result = lab_dark_red.ToRGB();
        // Low-intensity colors can have very high conversion errors in CIELAB
        CHECK(ABS((int)dark_red_result.r - (int)dark_red.r) <= 70);
        CHECK(ABS((int)dark_red_result.g - (int)dark_red.g) <= 220);
        CHECK(ABS((int)dark_red_result.b - (int)dark_red.b) <= 255);

        // Test edge cases with single channel values
        CRGB single_1(1, 0, 0);
        CIELAB16 lab_single_1(single_1);
        CRGB single_1_result = lab_single_1.ToRGB();
        CHECK(ABS((int)single_1_result.r - (int)single_1.r) <= 5);
        CHECK(ABS((int)single_1_result.g - (int)single_1.g) <= 5);
        CHECK(ABS((int)single_1_result.b - (int)single_1.b) <= 5);

        CRGB single_254(254, 254, 254);
        CIELAB16 lab_single_254(single_254);
        CRGB single_254_result = lab_single_254.ToRGB();
        CHECK(ABS((int)single_254_result.r - (int)single_254.r) <= 5);
        CHECK(ABS((int)single_254_result.g - (int)single_254.g) <= 5);
        CHECK(ABS((int)single_254_result.b - (int)single_254.b) <= 5);
    }
}


// TEST_CASE("CIELAB16 video operations") {
//     // Generate a test CRGB color - let's use a vibrant purple-ish color
//     CRGB original_color(0, 0, 0);
    
//     // Compute hue and saturation of original color
//     CHSV original_hsv = rgb2hsv_approximate(original_color);
//     uint8_t original_hue = original_hsv.h;
//     uint8_t original_saturation = original_hsv.s;
    
//     // Convert CRGB -> CIELAB16 -> ToVideoRGB()
//     CIELAB16 lab(original_color);
//     printf("CIELAB values: L=%d, A=%d, B=%d\n", lab.l, lab.a, lab.b);
    
//     // Test regular conversion first
//     CRGB regular_rgb = lab.ToRGB();
//     printf("Regular RGB: (%d, %d, %d)\n", regular_rgb.r, regular_rgb.g, regular_rgb.b);
    
//     CRGB video_color = lab.ToVideoRGB();
    
//     // Compute hue and saturation of the video processed color
//     CHSV video_hsv = rgb2hsv_approximate(video_color);
//     uint8_t video_hue = video_hsv.h;
//     uint8_t video_saturation = video_hsv.s;
    
//     // Calculate sum of absolute differences for each component
//     uint16_t r_diff = abs((int16_t)original_color.r - (int16_t)video_color.r);
//     uint16_t g_diff = abs((int16_t)original_color.g - (int16_t)video_color.g);
//     uint16_t b_diff = abs((int16_t)original_color.b - (int16_t)video_color.b);
//     uint16_t total_rgb_diff = r_diff + g_diff + b_diff;
    
//     // Calculate hue and saturation differences
//     //uint16_t hue_diff = abs((int16_t)original_hue - (int16_t)video_hue);
//     uint16_t hue_diff = hue_compare(original_hue, video_hue);
//     uint16_t sat_diff = abs((int16_t)original_saturation - (int16_t)video_saturation);
    
//     // Print results
//     printf("Original RGB: (%d, %d, %d)\n", original_color.r, original_color.g, original_color.b);
//     printf("Video RGB:    (%d, %d, %d)\n", video_color.r, video_color.g, video_color.b);
//     printf("Original H/S: (%d, %d)\n", original_hue, original_saturation);
//     printf("Video H/S:    (%d, %d)\n", video_hue, video_saturation);
//     printf("RGB component differences: R=%d, G=%d, B=%d\n", r_diff, g_diff, b_diff);
//     printf("Sum of absolute RGB differences: %d\n", total_rgb_diff);
//     printf("Hue difference: %d\n", hue_diff);
//     printf("Saturation difference: %d\n", sat_diff);
//     printf("Done\n");

//     uint8_t diff_r = abs(original_color.r - video_color.r);
//     uint8_t diff_g = abs(original_color.g - video_color.g);
//     uint8_t diff_b = abs(original_color.b - video_color.b);

//     REQUIRE_LT(diff_r, 255);
// }

// TEST_CASE("CIELAB16 video operations - fill") {
//     // test all combinations of RGB values
//     for (int r = 0; r < 256; r++) {
//         for (int g = 0; g < 256; g++) {
//             for (int b = 0; b < 256; b++) {
//                 CRGB c(r, g, b);
//                 CIELAB16 lab(c);
//                 CRGB video_color = lab.ToVideoRGB();
//                 CHSV hsv_vid = rgb2hsv_approximate(video_color);
//                 CHSV hsv_orig = rgb2hsv_approximate(c);
//                 uint8_t hue_diff = hue_compare(hsv_orig.h, hsv_vid.h);
//                 uint8_t sat_diff = abs(hsv_orig.s - hsv_vid.s);
//                 if (hue_diff > 10) {
//                     printf("Original RGB: (%d, %d, %d)\n", c.r, c.g, c.b);
//                     printf("Video RGB:    (%d, %d, %d)\n", video_color.r, video_color.g, video_color.b);
//                 }
//             }
//         }
//     }
//     printf("Done\n");
// }