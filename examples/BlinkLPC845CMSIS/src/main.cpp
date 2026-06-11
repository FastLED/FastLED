/// @file    Blink_LPC845.cpp
/// @brief   FastLED test for LPC845 - WS2812 strip blink
/// @example Blink_LPC845.cpp

// FL_AGENT_ALLOW_NEW_EXAMPLE

#include <FastLED.h>
#include <LPC845.h>

// How many leds in your strip?
#define NUM_LEDS 1

// GPIO pin for LED data line (adjust for your hardware)
#define PIN_DATA 3

// Define the array of leds
CRGB leds[NUM_LEDS];

// Setup function called once at startup
extern "C" void setup() {
    // Initialize FastLED - use WS2812/NEOPIXEL protocol
   //FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);

    // Set debug GPIO pin (optional, for testing)
    GPIO->DIRSET[0] = (1 << 0); // Set GPIO pin 0 as output
}

// Loop function called repeatedly
extern "C" void loop() {
    // Turn the LED on (red)
    leds[0] = CRGB::Red;
   // FastLED.show();
    fl::delay(500);

    // Debug toggle a GPIO pin (optional, for testing)
    GPIO->NOT[0] = (1 << 0); // Toggle GPIO pin 0

    // Turn the LED off
    leds[0] = CRGB::Black;
  //  FastLED.show();
    fl::delay(100);
}
