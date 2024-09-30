
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


using namespace std;

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


void _test(uint8_t* _brightness, uint8_t* _v5) {
    const uint8_t brightness = *_brightness;
    uint8_t v5 = *_v5;
    uint32_t numerator = 1;
    uint16_t denominator = 1;  // can hold all possible denominators for v5.
    // Loop while there is room to adjust brightness
    if (v5 > 1) {
        // Calculate the next reduced value of v5
        uint8_t next_v5 = v5 >> 1;
        // Update the numerator and denominator to scale brightness
        uint32_t next_numerator = numerator * v5;
        uint32_t next_denominator = denominator * next_v5;
        // Calculate the next potential brightness value
        uint32_t next_brightness_times_numerator = brightness;
        next_brightness_times_numerator *= next_numerator;
        // Check for overflow
        if (next_brightness_times_numerator >= denominator * 0xff) {
            return;
        }

        numerator = next_numerator;
        denominator = next_denominator;
        v5 = next_v5;
    }

    uint32_t b32 = brightness;
    b32 *= numerator;
    *_brightness = static_cast<uint8_t>(b32 / denominator);
    *_v5 = v5;

}

uint8_t multiply(uint8_t b, uint8_t v5) {
  float bf, vf;
  bf = float(b) / 255;
  vf = float(v5) / 31;
  float r = bf * vf;
  return round(r * 255);
}

uint8_t multiply16(uint16_t c , uint8_t v5) {
  double cf, vf;
  cf = double(c) / 0xffff;
  vf = double(v5) / 31;
  double r = cf * vf;
  return round(r * 0xff);
}


TEST_CASE("five_bit_bitshift") {

  #if 0
  SUBCASE("full brightness") {
    uint8_t brightness = 255;
    uint8_t v5 = 31;

    five_bit_bitshift_brightness(&brightness, &v5);
    CHECK_EQ(brightness, 255);
    CHECK_EQ(v5, 31);
  }

  SUBCASE("min brightness") {
    uint8_t brightness = 1;
    uint8_t v5 = 31;

    five_bit_bitshift_brightness(&brightness, &v5);
    CHECK_EQ(brightness, 31);
    CHECK_EQ(v5, 1);
  }
  #endif

  SUBCASE("random") {
    for (int i = 0; i < 10000; ++i) {
      uint8_t brightness = rand() % 256;
      uint8_t v5 = rand() % 32;

      uint8_t results = multiply(brightness, v5);
      five_bit_bitshift_brightness(&brightness, &v5);
      uint8_t results2 = multiply(brightness, v5);
      CHECK_EQ(results, results2);
    }
  }

  SUBCASE("random color swapping") {
    for (int i = 0; i < 100000; ++i) {
      uint16_t r16 = rand() % (0xffff + 1);
      uint16_t g16 = rand() % (0xffff + 1);
      uint16_t b16 = rand() % (0xffff + 1);
      uint8_t v5 = rand() % 32;

      uint8_t r8 = multiply16(r16, v5);
      uint8_t g8 = multiply16(g16, v5);
      uint8_t b8 = multiply16(b16, v5);

      five_bit_color_bitshift(&r16, &g16, &b16, &v5);

      uint8_t r8_2 = multiply16(r16, v5);
      uint8_t g8_2 = multiply16(g16, v5);
      uint8_t b8_2 = multiply16(b16, v5);

      //INFO("i: " << i << " r16: " << r16 << " g16: " << g16 << " b16: " << b16 << " v5: " << v5);
      INFO("i: " << i << " r16: " << r16 << " r8: " << int(r8) << " g16: " << g16 << " g8: " << int(g8) << " b16: " << b16 << " b8: " << int(b8));

      CHECK_EQ(r8, r8_2);
      CHECK_EQ(g8, g8_2);
      CHECK_EQ(b8, b8_2);

      

    }
  }

  SUBCASE("small brightness") {
    uint8_t brightness = 6;
    uint8_t v5 = 31;

    uint8_t results = multiply(brightness, v5);

    bool swapped = five_bit_bitshift_brightness(&brightness, &v5);
    CHECK_EQ(swapped, true);

    uint8_t results2 = multiply(brightness, v5);

    CHECK_EQ(results2, results);

    #if 0

    uint8_t results = multiply(brightness, v5);
    CHECK_EQ(6, results);

    _test(&brightness, &v5);
    results = multiply(brightness, v5);
    CHECK_EQ(12, brightness);
    CHECK_EQ(15, v5);
    CHECK_EQ(6, results);

    _test(&brightness, &v5);
    results = multiply(brightness, v5);
    CHECK_EQ(25, brightness);
    CHECK_EQ(7, v5);
    CHECK_EQ(6, results);

    _test(&brightness, &v5);
    results = multiply(brightness, v5);
    CHECK_EQ(58, brightness);
    CHECK_EQ(3, v5);
    CHECK_EQ(6, results);

    _test(&brightness, &v5);
    results = multiply(brightness, v5);
    CHECK_EQ(174, brightness);
    CHECK_EQ(1, v5);
    CHECK_EQ(6, results);
    #endif



    // five_bit_bitshift_brightness(&brightness, &v5);
    // CHECK_EQ(brightness, 5 * 31);
    // CHECK_EQ(v5, 15);

    //_test(&brightness, &v5);
    //CHECK_EQ(brightness, 5 * 31);
    
  }

}

#if 0
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

#endif