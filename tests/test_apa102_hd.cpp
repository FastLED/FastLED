
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "five_bit_hd_gamma.h"

TEST_CASE("sanity check") {
  CHECK(1 == 1);
}

TEST_CASE("five_bit_hd_gamma_bitshift functionality") {
  uint8_t r8, g8, b8, power_5bit;
  
  SUBCASE("Full brightness, no scaling") {
    five_bit_hd_gamma_bitshift(255, 255, 255, 255, 255, 255, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 255);
    CHECK_EQ(g8, 255);
    CHECK_EQ(b8, 255);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Full brightness, half scaling") {
    five_bit_hd_gamma_bitshift(255, 255, 255, 128, 128, 128, &r8, &g8, &b8, &power_5bit);
    CHECK_LT(r8, 255);
    CHECK_EQ(g8, r8);
    CHECK_EQ(b8, r8);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Different colors, no scaling") {
    five_bit_hd_gamma_bitshift(255, 128, 64, 255, 255, 255, &r8, &g8, &b8, &power_5bit);
    CHECK_EQ(r8, 255);
    CHECK_GT(g8, 0);
    CHECK_LT(g8, 255);
    CHECK_GT(b8, 0);
    CHECK_LT(b8, g8);
    CHECK_EQ(power_5bit, 31);
  }

  SUBCASE("Some different values") {
    five_bit_hd_gamma_bitshift(255, 128, 64, 128, 64, 32, &r8, &g8, &b8, &power_5bit);
    CHECK_GT(r8, 0);
    CHECK_LT(r8, 255);
    CHECK_GT(g8, 0);
    CHECK_LT(g8, r8);
    CHECK_GT(b8, 0);
    CHECK_LT(b8, g8);
    CHECK_EQ(power_5bit, 31);
  }
}

