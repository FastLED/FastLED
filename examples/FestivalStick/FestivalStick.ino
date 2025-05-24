
#include <FastLED.h>
#include "fl/screenmap.h"

#define NUM_LEDS 288
#define DATA_PIN 6
#define CM_BETWEEN_LEDS 1.0  // 1cm between LEDs
#define CM_LED_DIAMETER 0.5  // 0.5cm LED diameter

CRGB leds[NUM_LEDS];
fl::ScreenMap circleMap = fl::ScreenMap::Circle(NUM_LEDS, CM_BETWEEN_LEDS, CM_LED_DIAMETER);

void setup() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)
    .setScreenMap(circleMap);
  FastLED.setBrightness(64);
}

void loop() {
  // Fill the strip with rainbow colors
  fill_rainbow(leds, NUM_LEDS, millis() / 20, 7);
  
  // Add some sparkles
  if (random8() < 32) {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
  
  // Rotate the circle pattern
  static uint8_t rotation = 0;
  rotation++;
  
  // Example of using the circle map (optional visual effect)
  for (int i = 0; i < NUM_LEDS; i++) {
    // Get the 2D position of this LED
    fl::vec2f pos = circleMap[i];
    
    // Calculate distance from center (normalized)
    fl::vec2f bounds = circleMap.getBounds();
    float centerX = bounds.x / 2.0f;
    float centerY = bounds.y / 2.0f;
    float dx = pos.x - centerX;
    float dy = pos.y - centerY;
    float dist = sqrt(dx*dx + dy*dy) / centerX;  // Normalized 0-1
    
    // Add a ripple effect based on distance from center
    uint8_t hue = dist * 255 + rotation;
    leds[i] += CHSV(hue, 240, 128 * (1.0f - dist));
  }
  
  FastLED.show();
  FastLED.delay(15);
}
