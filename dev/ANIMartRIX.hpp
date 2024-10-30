/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino


#include "fx/2d/animartrix.hpp"

#include <Arduino.h>
#include "FastLED.h"

#include <iostream>

#define WIDTH 22                      // how many columns?
#define HEIGHT  22                       // how many rows?


#define DEBUG_PRINT 0

#define NUM_LED (WIDTH * HEIGHT)
#define SERPENTINE true

#define CYCLE_THROUGH_ANIMATIONS 10

CRGB leds[NUM_LED];               // framebuffer

XYMap xyMap(WIDTH, HEIGHT, SERPENTINE);
AnimartrixRef fxAnimator = AnimartrixRef::New(xyMap, POLAR_WAVES);

void setup() {
  FastLED.addLeds<WS2811, 2, GRB>(leds, NUM_LED);   
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000); // optional current limiting [5V, 2000mA] 
  Serial.begin(115200);                 // check serial monitor for current fps count
  // fill_rainbow(leds, NUM_LED, 0);
  fill_solid(leds, NUM_LED, CRGB::Black);
  FastLED.show();
  fxAnimator->lazyInit();  // test look up table construction.
}

void loop() {
  uint32_t now = millis();
  // Change animation every 10 seconds
  #if CYCLE_THROUGH_ANIMATIONS > 0
  EVERY_N_SECONDS(CYCLE_THROUGH_ANIMATIONS) {
    fxAnimator->fxNext();
    #if DEBUG_PRINT
    std::cout << "New animation: " << fxAnimator.fxName() << std::endl;
    #endif
  }
  #endif
  fxAnimator->draw(Fx::DrawContext{millis(), leds});
  FastLED.show();
  uint32_t elapsed = millis() - now;

  EVERY_N_SECONDS(1) {
    #if DEBUG_PRINT
    std::cout << "frame time: " << elapsed << "ms" << std::endl;
    #endif
  }
}
