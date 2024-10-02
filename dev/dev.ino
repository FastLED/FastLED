

#include <FastLED.h>


#define LED_PIN     2
#define COLOR_ORDER BRG
#define CHIPSET     WS2811
#define NUM_LEDS    100

CRGB leds[NUM_LEDS];

struct NoiseGenerator {
  NoiseGenerator() {
    iteration_scale = 10;
    time_multiplier = 10;
  }
  NoiseGenerator (int32_t itScale, int32_t timeMul) : iteration_scale(itScale), time_multiplier(timeMul) {
  }
  // i is the position of the LED
  uint8_t Value(int32_t i, unsigned long time_ms) const;
  int LedValue(int32_t i, unsigned long time_ms) const;
  int32_t iteration_scale;
  unsigned long time_multiplier;
};

uint8_t NoiseGenerator::Value(int32_t i, unsigned long time_ms) const {
  uint32_t input = iteration_scale*i + time_ms * time_multiplier;
  uint16_t v1 = inoise16(input);
  return uint8_t(v1 >> 8);
}

int NoiseGenerator::LedValue(int32_t i, unsigned long time_ms) const {
  int val = Value(i, time_ms);
  return max(0, val - 128) * 2;
}


void setup_noisewave();
int noisewave_loop(bool clear, bool sensor_active_top, bool sensor_active_bottom);


//#define VIS_DURATION 1000 * 60 * 10  // 10 minutes
#define VIS_DURATION 1000 * 5   // 5 seconds


void setup_noisewave() {

}

int noisewave_loop(bool clear, bool sensor_active_top, bool sensor_active_bottom) {
  unsigned long start_t = millis();
  unsigned long time_now = start_t;
  NoiseGenerator noiseGeneratorRed (500, 14);
  NoiseGenerator noiseGeneratorBlue (500, 10);
  for (int32_t i = 0; i < NUM_LEDS; ++i) {
    int r = noiseGeneratorRed.LedValue(i, time_now);
    int b = noiseGeneratorBlue.LedValue(i, time_now + 100000) >> 1;
    int g = 0;
    leds[i].r = r;
    leds[i].g = g;
    leds[i].b = b;
  }
  return 0;
}





void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();
  FastLED.setBrightness(128);
}

void loop()
{
  noisewave_loop(false, false, false);
  FastLED.delay(1000 / 60);
}

