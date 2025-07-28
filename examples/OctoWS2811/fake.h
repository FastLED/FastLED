#include <FastLED.h>

#define NUM_LEDS 60

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(9600);
  Serial.println("OctoWS2811 demo - fallback mode (requires Teensy)");
  // Use a single pin fallback for non-Teensy platforms
  FastLED.addLeds<WS2812, 2, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(60);
}

void loop() {
  // Simple color cycle for non-Teensy platforms
  static uint8_t hue = 0;
  fill_rainbow(leds, NUM_LEDS, hue, 255/NUM_LEDS);
  FastLED.show();
  delay(50);
  hue++;
}
