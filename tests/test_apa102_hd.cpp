
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
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
    __builtin_five_bit_hd_gamma_bitshift(CRGB(data[0][0], data[0][1], data[0][2]), CRGB(255, 255, 255), data[0][3], &out_color, &out_brightness);
    INFO("input  red ", data[0][0], " green ", data[0][1], " blue ", data[0][2], " brightness ", data[0][3]);
    INFO("output red ", out_color.r, " green ", out_color.g, " blue ", out_color.b, " brightness ", out_brightness);
    CHECK_EQ(out_color.r, data[1][0]);
    CHECK_EQ(out_color.g, data[1][1]);
    CHECK_EQ(out_color.b, data[1][2]);
    CHECK_EQ(out_brightness, data[1][3]);
  }
}

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

static float power_diff(Power power) {
  return abs(power.power - power.power_5bit);
}


static float power_rgb(CRGB color, uint8_t brightness) {
  color *= brightness;
  float out = color.r / 255.f + color.g / 255.f + color.b / 255.f;
  return out / 3.0f;
}

static float compute_power_5bit(CRGB color, uint8_t power_5bit, uint8_t brightness) {
  assert(power_5bit <= 31);
  float rgb_pow = power_rgb(color, brightness);
  float brightness_pow = (power_5bit) / 31.0f;

  float out = rgb_pow * brightness_pow;
  return out;
}


static float compute_power_apa102(CRGB color, uint8_t brightness, uint8_t* power_5bit) {
  uint16_t r16 = map8_to_16(color.r);
  uint16_t g16 = map8_to_16(color.g);
  uint16_t b16 = map8_to_16(color.b);
  CRGB out_colors;
  uint8_t v5 = 31;
  uint8_t post_brightness_scale = five_bit_bitshift(r16, g16, b16, brightness, &out_colors, power_5bit);
  float power = compute_power_5bit(out_colors, v5, post_brightness_scale);
  return power;
}


static float compute_power_ws2812(CRGB color, uint8_t brightness) {
  float power = power_rgb(color, brightness);
  return power;
}

static Power compute_power(uint8_t brightness8, CRGB color) {
  uint8_t power_5bit_u8;
  float power_5bit = compute_power_apa102(color, brightness8, &power_5bit_u8);
  float power_rgb = compute_power_ws2812(color, brightness8);
  return {power_rgb, power_5bit, power_5bit_u8};
}

static void make_random(CRGB* color, uint8_t* brightness) {
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

