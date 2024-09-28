
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "five_bit_hd_gamma.h"
#include "assert.h"
#include "math.h"
#include <ctime>
#include <cstdlib>


#define CHECK_NEAR(a, b, c) CHECK_LT(abs(a - b), c)


// Testing allows upto 25% error between power output of WS2812 and APA102 in HD mode.
// This probably happens on the high end of the brightness scale.
const static float TOLERANCE = 0.25;



struct Power {
  float power;
  float power_5bit;
};


uint16_t mymap(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return static_cast<uint16_t>(
     (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
  );
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


TEST_CASE("five_bit_hd_gamma_bitshift functionality") {
  enum {
    kBrightness = 255,
  };

  SUBCASE("Randomized Test") {
    srand(time(NULL));  // Seed the random number generator
    const int NUM_TESTS = 1000000;

    bool fail = false;
    
    for (int i = 0; i < NUM_TESTS; i++) {
      uint8_t brightness = rand() % 256;  // Random brightness (0-255)
      uint8_t r = rand() % 256;  // Random red value (0-255)
      uint8_t g = rand() % 256;  // Random green value (0-255)
      uint8_t b = rand() % 256;  // Random blue value (0-255)
      
      CRGB color(r, g, b);
      Power result = compute_power(brightness, color);
      float diff = abs(result.power - result.power_5bit);

      if (diff > .04) {
        std::cout << "diff=" << diff << " for brightness=" << (int)brightness << ", color=(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;
        fail = true;
      }
    }
    if (fail) {
      FAIL("Some tests failed");
    }
  }
}

