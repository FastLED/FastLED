/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

#include "fx/animartrix.hpp"

#define num_x  22                       // how many LEDs are in one row?
#define num_y  22                       // how many rows?
#define NUM_LED (num_x * num_y)
CRGB leds[NUM_LED];               // framebuffer
bool serpentine = true;
// FastLEDANIMartRIX animatrix(num_x, num_y, leds, serpentine);

AnimatrixData data(num_x, num_y, leds, RGB_BLOBS5, serpentine);



void setup() {
  FastLED.addLeds<WS2811, 2, GRB>(leds, NUM_LED);   
  FastLED.setMaxPowerInVoltsAndMilliamps( 5, 2000); // optional current limiting [5V, 2000mA] 
  Serial.begin(115200);                 // check serial monitor for current fps count
  // fill_rainbow(leds, NUM_LED, 0);
  fill_solid(leds, NUM_LED, CRGB::Black);
  FastLED.show();
}

void loop() {
  static Animation currentAnimation = RGB_BLOBS5;

  //animatrix.logOutput();

  //animatrix.logFrame();
  //EVERY_N_MILLIS(500) animatrix.report_performance();   // check serial monitor for report

  // Change animation every 10 seconds
  EVERY_N_SECONDS(10) {
    currentAnimation = static_cast<Animation>((static_cast<int>(currentAnimation) + 1) % NUM_ANIMATIONS);
    data.current_animation = currentAnimation;
  }

  AnimatrixDataLoop(data);
  FastLED.show();
}