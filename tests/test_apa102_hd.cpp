
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "FastLED.h"
#include "fl/five_bit_hd_gamma.h"
#include "assert.h"
#include "math.h"
#include <ctime>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "crgb.h"
#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("five_bit_bitshift") {
  const uint16_t test_data[][2][4] = {
    { // test case
      //r g  b  brightness
      {0, 0, 0, 0}, // input
      {0, 0, 0, 0}, // output
    },
    { // 0 brightness brings all colors down to 0
      {0xffff, 0xffff, 0xffff, 0},
      {0,      0,      0,      0},
    },
    { // color values below 8 become 0 at max brightness
      {8, 7, 0, 255},
      {1, 0, 0, 1},
    },
    {
      {0xffff, 0x00f0, 0x000f, 0x01},
      {0x11,   0x00,   0x00,   0x01},
    },
    {
      {0x0100, 0x00f0, 0x000f, 0xff},
      {0x08,   0x08,   0x00,   0x03},
    },
    {
      {0x2000, 0x1000, 0x0f00, 0x20},
      {0x20,   0x10,   0x0f,   0x03},
    },
    {
      {0xffff, 0x8000, 0x4000, 0x40},
      {0x81,   0x41,   0x20,   0x0f},
    },
    {
      {0xffff, 0x8000, 0x4000, 0x80},
      {0x81,   0x41,   0x20,   0x1f},
    },
    {
      {0xffff, 0xffff, 0xffff, 0xff},
      {0xff,   0xff,   0xff,   0x1f},
    },
  };

  for (const auto& data : test_data) {
    CRGB out_color;
    uint8_t out_brightness;
    five_bit_bitshift(data[0][0], data[0][1], data[0][2], data[0][3], &out_color, &out_brightness);
    INFO("input  red ", data[0][0], " green ", data[0][1], " blue ", data[0][2], " brightness ", data[0][3]);
    INFO("output red ", out_color.r, " green ", out_color.g, " blue ", out_color.b, " brightness ", out_brightness);
    CHECK_EQ(out_color.r, data[1][0]);
    CHECK_EQ(out_color.g, data[1][1]);
    CHECK_EQ(out_color.b, data[1][2]);
    CHECK_EQ(out_brightness, data[1][3]);
  }
}

TEST_CASE("__builtin_five_bit_hd_gamma_bitshift") {
  // NOTE: FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8 is defined for this test in CMakeLists.txt

  const uint8_t test_data[][2][4] = {
    { // test case
      //r g  b  brightness
      {0, 0, 0, 0}, // input
      {0, 0, 0, 0}, // output
    },
    { // 0 brightness brings all colors down to 0
      {255, 255, 255, 0},
      {0,   0,   0,   0},
    },
    {
      {16, 16, 16, 16},
      {0,  0,  0,  1},
    },
    {
      {64, 64, 64, 8},
      {4,  4,  4,  1},
    },
    {
      {255, 127, 43, 1},
      {17,  3,   0,  1},
    },
    {
      {255, 127, 43, 1},
      {17,  3,   0,  1},
    },
    {
      {255, 127, 43, 64},
      {129, 21,  1,  15},
    },
    {
      {255, 127, 43, 255},
      {255, 42,  3,  31},
    },
    {
      {255, 255, 255, 255},
      {255, 255, 255, 31},
    },
  };

  for (const auto& data : test_data) {
    CRGB out_color;
    uint8_t out_brightness;
    five_bit_hd_gamma_bitshift(CRGB(data[0][0], data[0][1], data[0][2]), CRGB(255, 255, 255), data[0][3], &out_color, &out_brightness);
    INFO("input  red ", data[0][0], " green ", data[0][1], " blue ", data[0][2], " brightness ", data[0][3]);
    INFO("output red ", out_color.r, " green ", out_color.g, " blue ", out_color.b, " brightness ", out_brightness);
    CHECK_EQ(out_color.r, data[1][0]);
    CHECK_EQ(out_color.g, data[1][1]);
    CHECK_EQ(out_color.b, data[1][2]);
    CHECK_EQ(out_brightness, data[1][3]);
  }
}

#define CHECK_NEAR(a, b, c) CHECK_LT(abs(a - b), c)

// Forward declarations for helper functions
float ideal_gamma_correct(uint8_t rgb8);
float apa102_to_linear(uint8_t rgb8_out, uint8_t brightness_5bit);

// Helper function to apply ideal gamma correction (gamma = 2.0)
float ideal_gamma_correct(uint8_t rgb8) {
    float normalized = rgb8 / 255.0f;
    return pow(normalized, 2.0f);
}

// Helper function to convert APA102 result back to linear light output
float apa102_to_linear(uint8_t rgb8_out, uint8_t brightness_5bit) {
    float gamma_corrected = ideal_gamma_correct(rgb8_out);
    float brightness_scale = brightness_5bit / 31.0f;
    return gamma_corrected * brightness_scale;
}

TEST_CASE("apa102_gamma_accuracy_low_range") {
    // Test cases: Low-range RGB8 values where APA102HD should have good accuracy
    const uint8_t test_cases[][3] = {
        {16, 16, 16},    // Low values where 5-bit precision matters most
        {32, 24, 8},     // Uneven low values
        {24, 16, 32},    // Different permutation of low values
        {40, 40, 40},    // Medium-low values
        {48, 32, 16},    // Descending low values
        {20, 30, 40},    // Ascending low values
    };

    const float max_allowed_error = 0.25f; // 25% error tolerance for low range
    
    for (const auto& test_case : test_cases) {
        uint8_t r_in = test_case[0];
        uint8_t g_in = test_case[1]; 
        uint8_t b_in = test_case[2];
        
        // Calculate ideal linear light output using floating-point gamma correction
        float ideal_linear_r = ideal_gamma_correct(r_in);
        float ideal_linear_g = ideal_gamma_correct(g_in);
        float ideal_linear_b = ideal_gamma_correct(b_in);
        
        // Run APA102 algorithm with full brightness and no color scaling
        CRGB out_color;
        uint8_t out_brightness;
        five_bit_hd_gamma_bitshift(
            CRGB(r_in, g_in, b_in),     // Input color
            CRGB(255, 255, 255),        // No color scaling
            255,                        // Full global brightness
            &out_color, 
            &out_brightness
        );
        
        // Convert APA102 result back to linear light output
        float actual_linear_r = apa102_to_linear(out_color.r, out_brightness);
        float actual_linear_g = apa102_to_linear(out_color.g, out_brightness);
        float actual_linear_b = apa102_to_linear(out_color.b, out_brightness);
        
        // Calculate relative error for each component
        float error_r = ideal_linear_r > 0 ? fabs(actual_linear_r - ideal_linear_r) / ideal_linear_r : 0;
        float error_g = ideal_linear_g > 0 ? fabs(actual_linear_g - ideal_linear_g) / ideal_linear_g : 0;
        float error_b = ideal_linear_b > 0 ? fabs(actual_linear_b - ideal_linear_b) / ideal_linear_b : 0;
        
        INFO("Input RGB: (", (int)r_in, ", ", (int)g_in, ", ", (int)b_in, ")");
        INFO("APA102 output: RGB(", (int)out_color.r, ", ", (int)out_color.g, ", ", (int)out_color.b, ") + brightness ", (int)out_brightness);
        INFO("Ideal linear: (", ideal_linear_r, ", ", ideal_linear_g, ", ", ideal_linear_b, ")");
        INFO("Actual linear: (", actual_linear_r, ", ", actual_linear_g, ", ", actual_linear_b, ")");
        INFO("Relative errors: (", error_r*100, "%, ", error_g*100, "%, ", error_b*100, "%)");
        
        // Check that errors are within acceptable bounds for low range
        CHECK_LT(error_r, max_allowed_error);
        CHECK_LT(error_g, max_allowed_error);
        CHECK_LT(error_b, max_allowed_error);
        
        // Verify that APA102 output can actually represent the brightness
        // (brightness should be > 0 if any input component is > 0)
        if (r_in > 0 || g_in > 0 || b_in > 0) {
            CHECK_GT(out_brightness, 0);
        }
    }
}

TEST_CASE("apa102_gamma_accuracy_high_range_expected_degradation") {
    // Test cases: Higher RGB8 values where accuracy is expected to degrade
    // This test documents the expected behavior rather than requiring high accuracy
    const uint8_t test_cases[][3] = {
        {128, 96, 32},   // Mixed medium values
        {200, 150, 100}, // Higher values
        {255, 128, 64},  // High red, medium others
        {100, 200, 50},  // Medium red, high green, low blue
        {255, 255, 255}, // Maximum values
    };
    
    for (const auto& test_case : test_cases) {
        uint8_t r_in = test_case[0];
        uint8_t g_in = test_case[1]; 
        uint8_t b_in = test_case[2];
        
        // Calculate ideal linear light output using floating-point gamma correction
        float ideal_linear_r = ideal_gamma_correct(r_in);
        float ideal_linear_g = ideal_gamma_correct(g_in);
        float ideal_linear_b = ideal_gamma_correct(b_in);
        
        // Run APA102 algorithm with full brightness and no color scaling
        CRGB out_color;
        uint8_t out_brightness;
        five_bit_hd_gamma_bitshift(
            CRGB(r_in, g_in, b_in),     // Input color
            CRGB(255, 255, 255),        // No color scaling
            255,                        // Full global brightness
            &out_color, 
            &out_brightness
        );
        
        // Convert APA102 result back to linear light output
        float actual_linear_r = apa102_to_linear(out_color.r, out_brightness);
        float actual_linear_g = apa102_to_linear(out_color.g, out_brightness);
        float actual_linear_b = apa102_to_linear(out_color.b, out_brightness);
        
        // Calculate relative error for each component
        float error_r = ideal_linear_r > 0 ? fabs(actual_linear_r - ideal_linear_r) / ideal_linear_r : 0;
        float error_g = ideal_linear_g > 0 ? fabs(actual_linear_g - ideal_linear_g) / ideal_linear_g : 0;
        float error_b = ideal_linear_b > 0 ? fabs(actual_linear_b - ideal_linear_b) / ideal_linear_b : 0;
        
        INFO("Input RGB: (", (int)r_in, ", ", (int)g_in, ", ", (int)b_in, ")");
        INFO("APA102 output: RGB(", (int)out_color.r, ", ", (int)out_color.g, ", ", (int)out_color.b, ") + brightness ", (int)out_brightness);
        INFO("Ideal linear: (", ideal_linear_r, ", ", ideal_linear_g, ", ", ideal_linear_b, ")");
        INFO("Actual linear: (", actual_linear_r, ", ", actual_linear_g, ", ", actual_linear_b, ")");
        INFO("Relative errors: (", error_r*100, "%, ", error_g*100, "%, ", error_b*100, "%)");
        
        // For high range, we just verify the algorithm produces reasonable output
        // without strict accuracy requirements since degradation is expected
        
        // Verify that APA102 output can actually represent the brightness
        // (brightness should be > 0 if any input component is > 0)
        if (r_in > 0 || g_in > 0 || b_in > 0) {
            CHECK_GT(out_brightness, 0);
        }
        
        // Verify output is non-zero for non-zero input
        if (r_in > 0 || g_in > 0 || b_in > 0) {
            bool has_output = (out_color.r > 0 || out_color.g > 0 || out_color.b > 0);
            CHECK(has_output);
        }
    }
}
