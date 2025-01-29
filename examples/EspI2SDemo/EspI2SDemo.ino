// Simple test for the I2S on the ESP32dev board.
// IMPORTANT:
//   This is using examples is built on esp-idf 4.x. This existed prior to Arduino Core 3.0.0.
//   To use this example, you MUST downgrade to Arduino Core < 3.0.0
//   or it won't work on Arduino.


#define FASTLED_ESP32_I2S
#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 1

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN 3

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() { 
    // Uncomment/edit one of the following lines for your leds arrangement.
    // ## Clockless types ##
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
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

  // This is a no-op but tests that we have access to gCntBuffer, part of the
  // i2s api. You can delete this in your own sketch. It's only here for testing
  // purposes.
  if (false) {
    int value = gCntBuffer;
    value++;
  }
}
