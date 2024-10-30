
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
#include <algorithm>
#include <iostream>
#include <sstream>

#include "namespace.h"
FASTLED_USING_NAMESPACE

#define CHECK_NEAR(a, b, c) CHECK_LT(abs(a - b), c)


#define STRESS_TEST 1

#define PROBLEMATIC_TEST 0

// Testing allows upto 21% error between power output of WS2812 and APA102 in HD mode.
// This is mostly due to the rounding errors for WS2812 when one of the channels is small
// and the rest are fairly large. One component will get rounded down to 0, while in
// apa mode it will bitshift to relevance.
const static float TOLERANCE = 0.21;
const static int NUM_TESTS = 10000;
const static size_t MAX_FAILURES = 30;
const static int CUTOFF = 11;  // WS2812 will give bad results when one of the components is less than 10.
struct Power {
  float power;
  float power_5bit;
  uint8_t power_5bit_u8;
};

float power_diff(Power power) {
  return abs(power.power - power.power_5bit);
}

uint16_t mymap(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return static_cast<uint16_t>(
     (x - in_min) * (out_max - out_min) / static_cast<double>(in_max - in_min) + out_min
  );
}

float power_rgb(CRGB color, uint8_t brightness) {
  color *= brightness;
  float out = color.r / 255.f + color.g / 255.f + color.b / 255.f;
  return out / 3.0f;
}

float compute_power_5bit(CRGB color, uint8_t power_5bit, uint8_t brightness) {
  assert(power_5bit <= 31);
  float rgb_pow = power_rgb(color, brightness);
  float brightness_pow = (power_5bit) / 31.0f;

  float out = rgb_pow * brightness_pow;
  return out;
}


float compute_power_apa102(CRGB color, uint8_t brightness, uint8_t* power_5bit) {
  uint16_t r16 = map8_to_16(color.r);
  uint16_t g16 = map8_to_16(color.g);
  uint16_t b16 = map8_to_16(color.b);
  CRGB out_colors;
  uint8_t v5 = 31;
  uint8_t post_brightness_scale = five_bit_bitshift(r16, g16, b16, brightness, &out_colors, power_5bit);
  float power = compute_power_5bit(out_colors, v5, post_brightness_scale);
  return power;
}


float compute_power_ws2812(CRGB color, uint8_t brightness) {
  float power = power_rgb(color, brightness);
  return power;
}

Power compute_power(uint8_t brightness8, CRGB color) {
  uint8_t power_5bit_u8;
  float power_5bit = compute_power_apa102(color, brightness8, &power_5bit_u8);
  float power_rgb = compute_power_ws2812(color, brightness8);
  return {power_rgb, power_5bit, power_5bit_u8};
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




void problematic_test(CRGB color, uint8_t brightness) {
    Power p = compute_power(brightness, color);
    std::ostringstream oss;

    // print out the power
    oss << "" << std::endl;
    oss << "power: " << p.power << " power_5bit: " << p.power_5bit << " power_5bit_u8: " << int(p.power_5bit_u8) << std::endl;
    oss << "brightness: " << int(brightness) << " color: R: " << int(color.r) << " G: " << int(color.g) << " B: " << int(color.b) << std::endl;
    oss << "compute_power_5bit: " << compute_power_5bit(color, p.power_5bit_u8, brightness) << std::endl;
    oss << "Power RGB: " << power_rgb(color, brightness) << std::endl;
    oss << "Diff: " << power_diff(p) << std::endl;
    std::cout << oss.str() << std::endl;
}


TEST_CASE("five_bit_hd_gamma_bitshift functionality") {

  SUBCASE("Sanity test for defines") {
    CHECK_EQ(FASTLED_HD_COLOR_MIXING, 1);
  }

#if PROBLEMATIC_TEST

  SUBCASE("problematic test2") {
    // Failure, diff is 0.580777 brightness: 249 color: R: 103 G: 125 B: 236 power: 0 power_5bit: 31
    CRGB color = {103, 125, 236};
    uint8_t brightness = 249;
    problematic_test(color, brightness);
    FAIL("Problematic test failed");
  }
#endif

#if STRESS_TEST
  SUBCASE("Randomized Power Matching Test for 5 bit power") {
    srand(0);  // Seed the random number generator so we get consitent results.
    bool fail = false;
    std::vector<Data> failures;
    for (int i = 0; i < NUM_TESTS; i++) {
      CRGB color;
      uint8_t brightness;
      make_random(&color, &brightness);
      // if one ore more of the compoents is less than 10 then skip.
      if (color.r < CUTOFF || color.g < CUTOFF || color.b < CUTOFF || brightness < CUTOFF) {
        // WS2812 does badly at this.
        continue;
      }
      Power result = compute_power(brightness, color);
      float diff = power_diff(result);
      if (diff > TOLERANCE) {
        failures.push_back({color, brightness});
        while (failures.size() > MAX_FAILURES) {
          // failures.pop_back();
          // select smallest power difference and remove it.
          auto it = std::min_element(failures.begin(), failures.end(), [](const Data& a, const Data& b) {
            Power p1 = compute_power(a.brightness, a.color);
            Power p2 = compute_power(b.brightness, b.color);
            return power_diff(p1) < power_diff(p2);
          });
          failures.erase(it);
        }
      }
    }
    if (failures.size()) {

      // sort by the power difference

      std::sort(failures.begin(), failures.end(), [](const Data& a, const Data& b) {
        Power p1 = compute_power(a.brightness, a.color);
        Power p2 = compute_power(b.brightness, b.color);
        return abs(p1.power - p1.power_5bit) > abs(p2.power - p2.power_5bit);
      });

      std::cout << "Failures:" << std::endl;
      for (auto& failure : failures) {
        Power p = compute_power(failure.brightness, failure.color);
        std::string color_str = "R: " + std::to_string(failure.color.r) + " G: " + std::to_string(failure.color.g) + " B: " + std::to_string(failure.color.b);
        std::cout << "Failure, diff is " << power_diff(p) << " brightness: " << int(failure.brightness) << " color: " << color_str << " power: " << p.power << " power_5bit: " << int(p.power_5bit_u8) << std::endl;
      }
      // FAIL("Failures found");
      // make a oostream object
      std::ostringstream oss;
      oss << __FILE__ << ":" << __LINE__ << " Failures found";
      FAIL(oss.str());
    }
  }
#endif

}

