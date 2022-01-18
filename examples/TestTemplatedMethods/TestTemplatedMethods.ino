#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 10
#define DATA_PIN 3

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() { 
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
}

void validate_colorutils() {
  // templated functions must be actually used, in order to exist in final binary.
  // templated functions can have errors, until/unless actually used.
  // therefore, cause each of the templated functions to be instantiated and called.

  fill_solid(leds, NUM_LEDS, CRGB::Red);
  fill_solid2(leds, 0, NUM_LEDS, CRGB::Green);
  
  // fill_gradient() -- four versions of this function
  // fill_gradient_RGB() -- four versions of this function
  // fadeUsingColor()
  // blend()
  // nblend()
  // blur1d()
  // blur2d() -- calls blurRows() and blurColumns()
  // fill_palette()
  // map_data_into_colors_through_palette()
  // fadeLightBy()
  // fade_video()
  // nscale8_video()
  // fadeToBlackBy()
  // fade_raw()
  // nscale8()
  // napplyGamma_video()
}

void loop() { 


  // Turn the LED on, then pause
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);
  // Now turn the LED off, then pause
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(500);
}
