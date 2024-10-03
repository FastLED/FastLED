/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

#include "fx/animartrix.hpp"

#include <iostream>

#define WIDTH  22                       // how many LEDs are in one row?
#define HEIGHT  22                       // how many rows?
#define NUM_LED (WIDTH * HEIGHT)
#define SERPENTINE true

#define CYCLE_THROUGH_ANIMATIONS

CRGB leds[NUM_LED];               // framebuffer

AnimartrixData data(WIDTH, HEIGHT, leds, POLAR_WAVES, SERPENTINE);

void setup() {
  FastLED.addLeds<WS2811, 2, GRB>(leds, NUM_LED);   
  FastLED.setMaxPowerInVoltsAndMilliamps( 5, 2000); // optional current limiting [5V, 2000mA] 
  Serial.begin(115200);                 // check serial monitor for current fps count
  // fill_rainbow(leds, NUM_LED, 0);
  fill_solid(leds, NUM_LED, CRGB::Black);
  FastLED.show();
}

void loop() {
  uint32_t now = millis();
  // Change animation every 10 seconds
  #if defined(CYCLE_THROUGH_ANIMATIONS)
  EVERY_N_SECONDS(1) {
    data.next();
    std::cout << "New animation: " << data.getName() << std::endl;
  }
  #endif
  AnimartrixLoop(data);
  FastLED.show();
  uint32_t elapsed = millis() - now;

  EVERY_N_SECONDS(1) {
    std::cout << "frame time: " << elapsed << "ms" << std::endl;
  }
}