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
        CRGB c(255, 255, 255);
        CIELAB16 lab(c);
        CRGB c2 = lab.ToRGB();
        CHECK(c2.r == c.r);
        CHECK(c2.g == c.g);
        CHECK(c2.b == c.b);
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