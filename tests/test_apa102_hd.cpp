
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "five_bit_hd_gamma.h"

TEST_CASE("FASTLED_APA102_USES_HD_GLOBAL_BRIGHTNESS is 1") {
  CHECK_EQ(FASTLED_HD_COLOR_MIXING, 1);
  CHECK_EQ(FASTLED_APA102_USES_HD_GLOBAL_BRIGHTNESS, 1);
}

TEST_CASE("five_bit_hd_gamma_bitshift functionality") {
  enum {
    kBrightness = 255,
  };
  uint8_t r8, g8, b8, power_5bit;
  
  SUBCASE("Full brightness, no scaling") {
    CRGB colors(255, 255, 255);
    CRGB colors_scale(255, 255, 255);
    CRGB out_colors;
    five_bit_hd_gamma_bitshift(colors, colors_scale, kBrightness, &out_colors, &power_5bit);
    CHECK_EQ(out_colors.r, 255);
    CHECK_EQ(out_colors.g, 255);
    CHECK_EQ(out_colors.b, 255);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Full brightness, half scaling") {
    CRGB colors(255, 255, 255);
    CRGB colors_scale(128, 128, 128);
    CRGB out_colors;
    five_bit_hd_gamma_bitshift(colors, colors_scale, kBrightness, &out_colors, &power_5bit);
    CHECK_LT(out_colors.r, 255);
    CHECK_EQ(out_colors.g, out_colors.r);
    CHECK_EQ(out_colors.b, out_colors.r);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Different colors, no scaling") {
    CRGB colors(255, 128, 64);
    CRGB colors_scale(255, 255, 255);
    CRGB out_colors;
    five_bit_hd_gamma_bitshift(colors, colors_scale, kBrightness, &out_colors, &power_5bit);
    CHECK_EQ(out_colors.r, 255);
    CHECK_GT(out_colors.g, 0);
    CHECK_LT(out_colors.g, 255);
    CHECK_GT(out_colors.b, 0);
    CHECK_LT(out_colors.b, out_colors.g);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Some different values") {
    CRGB colors(65, 64, 64);
    CRGB colors_scale(255, 255, 255);
    CRGB out_colors;
    five_bit_hd_gamma_bitshift(colors, colors_scale, kBrightness, &out_colors, &power_5bit);
    CHECK_EQ(out_colors.r, 170);
    CHECK_EQ(out_colors.g, 165);
    CHECK_EQ(out_colors.b, 165);
    CHECK_EQ(power_5bit, 3);
  }

  SUBCASE("Global brightness half") {
    const uint8_t brightness = 128;
    CRGB colors(65, 64, 64);
    CRGB colors_scale(255, 255, 255);
    CRGB out_colors;
    five_bit_hd_gamma_bitshift(colors, colors_scale, brightness, &out_colors, &power_5bit);
    CHECK_EQ(out_colors.r, 85);
    CHECK_EQ(out_colors.g, 83);
    CHECK_EQ(out_colors.b, 83);
    CHECK_EQ(power_5bit, 3);
  }

  SUBCASE("Global brightness low end") {
    const uint8_t brightness = 8;
    CRGB colors(65, 64, 64);
    CRGB colors_scale(255, 255, 255);
    CRGB out_colors;
    five_bit_hd_gamma_bitshift(colors, colors_scale, brightness, &out_colors, &power_5bit);
    CHECK_EQ(out_colors.r, 9);
    CHECK_EQ(out_colors.g, 9);
    CHECK_EQ(out_colors.b, 9);
    CHECK_EQ(power_5bit, 1);
  }

}

