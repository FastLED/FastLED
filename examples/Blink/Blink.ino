/// @file    Blink.ino
/// @brief   Blink the first LED of an LED strip
/// @example Blink.ino

#include <Arduino.h>
#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 1

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define PIN_DATA.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both PIN_DATA and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#ifndef PIN_DATA
#define PIN_DATA 3
#endif  // PIN_DATA

#define CLOCK_PIN 13

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() {

    // Uncomment/edit one of the following lines for your leds arrangement.
    // ## Clockless types ##
    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);  // GRB ordering is assumed

    //Serial.println("BLINK setup complete");
    // FastLED.addLeds<SM16824E, PIN_DATA, RGB>(leds, NUM_LEDS);  // RGB ordering (uses SM16824EController)
    // FastLED.addLeds<SM16703, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<TM1829, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<TM1812, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<TM1809, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<TM1804, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<TM1803, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<UCS1903, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<UCS1903B, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<UCS1904, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<UCS2903, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<UCS7604, PIN_DATA, GRB>(leds, NUM_LEDS);  // 8-bit RGBW chipset (all platforms)
    // FastLED.addLeds<UCS7604HD, PIN_DATA, GRB>(leds, NUM_LEDS);  // 16-bit RGBW chipset (all platforms)
    // FastLED.addLeds<WS2812, PIN_DATA, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    // FastLED.addLeds<WS2852, PIN_DATA, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    // FastLED.addLeds<WS2812B, PIN_DATA, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    // FastLED.addLeds<WS2812BV5, PIN_DATA, RGB>(leds, NUM_LEDS);  // GRB ordering is typical (newer V5 variant with tighter timing)
    // FastLED.addLeds<WS2812BMiniV3, PIN_DATA, RGB>(leds, NUM_LEDS);  // GRB ordering is typical (same timing as V5)
    // FastLED.addLeds<GS1903, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<SK6812, PIN_DATA, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    // FastLED.addLeds<SK6822, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<APA106, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<PL9823, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<SK6822, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<WS2811, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<WS2813, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<APA104, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<WS2811_400, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<GE8822, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<GW6205, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<GW6205_400, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<LPD1886, PIN_DATA, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<LPD1886_8BIT, PIN_DATA, RGB>(leds, NUM_LEDS);
    // ## Clocked (SPI) types ##
    // FastLED.addLeds<LPD6803, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    // FastLED.addLeds<LPD8806, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    // FastLED.addLeds<WS2801, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<WS2803, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<SM16716, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    // FastLED.addLeds<P9813, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // BGR ordering is typical
    // FastLED.addLeds<DOTSTAR, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // BGR ordering is typical
    // FastLED.addLeds<APA102, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // BGR ordering is typical
    // FastLED.addLeds<SK9822, PIN_DATA, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // BGR ordering is typical
}

void loop() {
  // Turn the LED on, then pause
  for (int i = 0; i < NUM_LEDS; ++i) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  delay(500);
  
  // Now turn the LED off, then pause
  for (int i = 0; i < NUM_LEDS; ++i) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
  delay(100);
}
