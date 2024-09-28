
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "five_bit_hd_gamma.h"
#include "assert.h"
#include "math.h"

#define CHECK_NEAR(a, b, c) CHECK_LT(abs(a - b), c)

uint16_t mymap(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

TEST_CASE("FASTLED_APA102_USES_HD_GLOBAL_BRIGHTNESS is 1") {
  CHECK_EQ(FASTLED_HD_COLOR_MIXING, 1);
  CHECK_EQ(FASTLED_APA102_USES_HD_GLOBAL_BRIGHTNESS, 1);
}

float power_rgb(CRGB color) {
  float out = uint16_t(color.r) + uint16_t(color.g) + uint16_t(color.b);
  return out / (255 * 3);
}

float computer_power_5bit(CRGB color, uint8_t five_bit_brightness) {
  assert(five_bit_brightness <= 31);
  float rgb_pow = power_rgb(color);
  float brightness_pow = five_bit_brightness / 31.0f;
  return rgb_pow * brightness_pow;
}

struct Power {
  float power;
  float power_5bit;
};


Power compute_power(uint8_t brightness8, CRGB color) {
  uint16_t r16 = mymap(color.r, 0, 255, 0, 0xffff);
  uint16_t g16 = mymap(color.g, 0, 255, 0, 0xffff);
  uint16_t b16 = mymap(color.b, 0, 255, 0, 0xffff);
  uint8_t power_5bit;
  CRGB out_colors;
  five_bit_bitshift(r16, g16, b16, brightness8, &out_colors, &power_5bit);
  float power = computer_power_5bit(out_colors, power_5bit);

  // Calculate power using rgb8 method
  CRGB color_rgb8 = color;
  color_rgb8.r = scale8(color_rgb8.r, brightness8);
  color_rgb8.g = scale8(color_rgb8.g, brightness8);
  color_rgb8.b = scale8(color_rgb8.b, brightness8);
  float power_rgb8 = power_rgb(color_rgb8);

  return {power, power_rgb8};
}

const static float TOLERANCE = 0.02;


TEST_CASE("five_bit_hd_gamma_bitshift functionality") {
  enum {
    kBrightness = 255,
  };

  auto verify_power = [](uint8_t brightness, CRGB color, float tolerance) {
    Power result = compute_power(brightness, color);
    CHECK_NEAR(result.power, result.power_5bit, tolerance);
  };

  SUBCASE("Max Brightness") {
    verify_power(255, CRGB(255, 255, 255), TOLERANCE);
  }

  SUBCASE("Half Brightness") {
    verify_power(127, CRGB(255, 255, 255), TOLERANCE);
  }

  SUBCASE("Low Brightness") {
    verify_power(8, CRGB(255, 255, 255), TOLERANCE);
  }

  SUBCASE("Low Color Brightness") {
    verify_power(255, CRGB(3, 3, 3), TOLERANCE);
  }

  SUBCASE("Low Color and Brightness") {
    verify_power(8, CRGB(3, 3, 3), TOLERANCE);
  }


}

