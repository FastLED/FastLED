/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

#include "fx/animartrix.hpp"

#define num_x  22                       // how many LEDs are in one row?
#define num_y  22                       // how many rows?
#define NUM_LED (num_x * num_y)
CRGB leds[NUM_LED];               // framebuffer
bool serpentine = true;
FastLEDANIMartRIX animatrix(num_x, num_y, leds, serpentine);

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

  switch (currentAnimation) {
    case RGB_BLOBS5: animatrix.RGB_Blobs5(); break;
    case RGB_BLOBS4: animatrix.RGB_Blobs4(); break;
    case RGB_BLOBS3: animatrix.RGB_Blobs3(); break;
    case RGB_BLOBS2: animatrix.RGB_Blobs2(); break;
    case RGB_BLOBS: animatrix.RGB_Blobs(); break;
    case POLAR_WAVES: animatrix.Polar_Waves(); break;
    case SLOW_FADE: animatrix.Slow_Fade(); break;
    case ZOOM2: animatrix.Zoom2(); break;
    case ZOOM: animatrix.Zoom(); break;
    case HOT_BLOB: animatrix.Hot_Blob(); break;
    case SPIRALUS2: animatrix.Spiralus2(); break;
    case SPIRALUS: animatrix.Spiralus(); break;
    case YVES: animatrix.Yves(); break;
    case SCALEDEMO1: animatrix.Scaledemo1(); break;
    case LAVA1: animatrix.Lava1(); break;
    case CALEIDO3: animatrix.Caleido3(); break;
    case CALEIDO2: animatrix.Caleido2(); break;
    case CALEIDO1: animatrix.Caleido1(); break;
    case DISTANCE_EXPERIMENT: animatrix.Distance_Experiment(); break;
    case CENTER_FIELD: animatrix.Center_Field(); break;
    case WAVES: animatrix.Waves(); break;
    case CHASING_SPIRALS: animatrix.Chasing_Spirals(); break;
    case ROTATING_BLOB: animatrix.Rotating_Blob(); break;
    case RINGS: animatrix.Rings(); break;
  }

  //animatrix.logOutput();
  FastLED.show();
  //animatrix.logFrame();
  //EVERY_N_MILLIS(500) animatrix.report_performance();   // check serial monitor for report

  // Change animation every 10 seconds
  EVERY_N_SECONDS(10) {
    currentAnimation = static_cast<Animation>((static_cast<int>(currentAnimation) + 1) % NUM_ANIMATIONS);
  }
}