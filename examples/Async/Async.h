// @file examples/TaskExample/TaskExample.ino
// @brief Example showing how to use the fl::task API

#include "FastLED.h"
#include "fl/task.h"
#include "fl/async.h"

#define NUM_LEDS 60
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
CRGB current_color = CRGB::Black;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(16);

    // Create a task that runs every 100ms to change color
    auto colorPicker = fl::task::every_ms(100, FL_TRACE)
      .then([] {
          current_color = CHSV(random8(), 255, 255);
      });

    // Create a task that runs at 60fps to update the LEDs
    auto displayTask = fl::task::at_framerate(60, FL_TRACE)
      .then([] {
          fill_solid(leds, NUM_LEDS, current_color);
          FastLED.show();
      });
}

void loop() {    
    // Yield to allow other operations to run
    fl::async_run();
}
