
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "five_bit_hd_gamma.h"
#include "assert.h"
#include "math.h"
#include <ctime>
#include <cstdlib>
#include <vector>

#define CHECK_NEAR(a, b, c) CHECK_LT(abs(a - b), c)

// Testing allows upto 0.6% error between power output of WS2812 and APA102 in HD mode.
const static float TOLERANCE = 0.006;
const static int NUM_TESTS = 1000000;
const static size_t MAX_FAILURES = 30;
struct Power {
  float power;
  float power_5bit;
};

uint16_t mymap(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return static_cast<uint16_t>(
     (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
  );
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

Power compute_power(uint8_t brightness8, CRGB color) {
  uint16_t r16 = mymap(color.r, 0, 255, 0, 0xffff);
  uint16_t g16 = mymap(color.g, 0, 255, 0, 0xffff);
  uint16_t b16 = mymap(color.b, 0, 255, 0, 0xffff);
  uint8_t power_5bit;
  CRGB out_colors;
  five_bit_bitshift(r16, g16, b16, brightness8, &out_colors, &power_5bit);
  float power = computer_power_5bit(out_colors, power_5bit);
  CRGB color_rgb8 = color;
  color_rgb8.r = scale8(color_rgb8.r, brightness8);
  color_rgb8.g = scale8(color_rgb8.g, brightness8);
  color_rgb8.b = scale8(color_rgb8.b, brightness8);
  float power_rgb8 = power_rgb(color_rgb8);
  return {power, power_rgb8};
}

void make_random(CRGB* color, uint8_t* brightness) {
  color->r = rand() % 256;
  color->g = rand() % 256;
  color->b = rand() % 256;
  *brightness = rand() % 256;
}

struct Data {
  CRGB color;
  uint8_t brightness;
};



TEST_CASE("five_bit_hd_gamma_bitshift functionality") {

  SUBCASE("Sanity test for defines") {
    CHECK_EQ(FASTLED_HD_COLOR_MIXING, 1);
  }

  SUBCASE("Randomized Power Matching Test for 5 bit power") {
    srand(0);  // Seed the random number generator so we get consitent results.
    bool fail = false;
    std::vector<Data> failures;
    for (int i = 0; i < NUM_TESTS; i++) {
      CRGB color;
      uint8_t brightness;
      make_random(&color, &brightness);
      Power result = compute_power(brightness, color);
      float diff = abs(result.power - result.power_5bit);
      if (diff > TOLERANCE) {
        failures.push_back({color, brightness});
        while (failures.size() > MAX_FAILURES) {
          failures.pop_back();
        }
      }
    }
    if (failures.size()) {
      std::cout << "Failures:" << std::endl;
      for (auto& failure : failures) {
        Power p = compute_power(failure.brightness, failure.color);
        std::string color_str = "R: " + std::to_string(failure.color.r) + " G: " + std::to_string(failure.color.g) + " B: " + std::to_string(failure.color.b);
        std::cout << "Failure, diff is " << abs(p.power - p.power_5bit) << " brightness: " << int(failure.brightness) << " color: " << color_str << std::endl;
      }
    }
  }
}

