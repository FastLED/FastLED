
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "five_bit_hd_gamma.h"

TEST_CASE("sanity check") {
  CHECK(1 == 1);
}

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
    five_bit_hd_gamma_bitshift(255, 255, 255, 255, 255, 255, kBrightness, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 255);
    CHECK_EQ(g8, 255);
    CHECK_EQ(b8, 255);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Full brightness, half scaling") {
    five_bit_hd_gamma_bitshift(255, 255, 255, 128, 128, 128, kBrightness, &r8, &g8, &b8, &power_5bit);
    CHECK_LT(r8, 255);
    CHECK_EQ(g8, r8);
    CHECK_EQ(b8, r8);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Different colors, no scaling") {
    five_bit_hd_gamma_bitshift(255, 128, 64, 255, 255, 255, kBrightness, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 255);
    CHECK_GT(g8, 0);
    CHECK_LT(g8, 255);
    CHECK_GT(b8, 0);
    CHECK_LT(b8, g8);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Some different values") {
    five_bit_hd_gamma_bitshift(65, 64, 64, 255, 255, 255, kBrightness, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 170);
    CHECK_EQ(g8, 165);
    CHECK_EQ(b8, 165);
    CHECK_EQ(power_5bit, 3);
  }

  SUBCASE("Global brightness half") {
    const uint8_t brightness = 128;
    five_bit_hd_gamma_bitshift(65, 64, 64, 255, 255, 255, brightness, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 85);
    CHECK_EQ(g8, 83);
    CHECK_EQ(b8, 83);
    CHECK_EQ(power_5bit, 3);
  }

  SUBCASE("Global brightness low") {
    const uint8_t brightness = 8;
    five_bit_hd_gamma_bitshift(65, 64, 64, 255, 255, 255, brightness, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 5);
    CHECK_EQ(g8, 5);
    CHECK_EQ(b8, 5);
    CHECK_EQ(power_5bit, 3);
  }

}

