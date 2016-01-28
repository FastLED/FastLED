// Use if you want to force the software SPI subsystem to be used for some reason (generally, you don't)
// #define FASTLED_FORCE_SOFTWARE_SPI
// Use if you want to force non-accelerated pin access (hint: you really don't, it breaks lots of things)
// #define FASTLED_FORCE_SOFTWARE_SPI
// #define FASTLED_FORCE_SOFTWARE_PINS
#include "FastLED.h"

///////////////////////////////////////////////////////////////////////////////////////////
//
// Move a white dot along the strip of leds.  This program simply shows how to configure the leds,
// and then how to turn a single pixel white and then off, moving down the line of pixels.
//

// NUM_LEDS:
//   Set NUM_LEDS to the number of LEDs on your strip.
//
// LED_TYPE:
//   Set LED_TYPE to one of the chipset constants below. Make note of whether it is
//   a 3-Wire or SPI chipset and uncomment the appropriate line in setup().
//   3-Wire Chipsets:
//   TM1803
//   TM1804
//   TM1809
//   WS2811
//   WS2812
//   WS2812B
//   NEOPIXEL
//   APA104
//   UCS1903
//   UCS1903B
//   GW6205
//   GW6205_400
//
//   SPI Chipsets:
//   WS2801
//   SM16716
//   LPD8806
//   P9813
//   APA102
//   DOTSTAR
//
// DATA_PIN:
//   For 3-Wire chipsets, set DATA_PIN to the pin number where the data line is
//   connected. For SPI chipsets, if you are not using the default hardware SPI
//   pins and are specifying your own, set DATA_PIN to the pin number where the
//   data line is connected.
//
// CLOCK_PIN:
//   For SPI chipsets, if you are specifying your own pins, also
//   set CLOCK_PIN to the pin number where the clock line is connected.
//
// COLOR_ORDER:
//   Set COLOR_ORDER to the order the controller should send the color data out.
//   In most cases leave this as RGB.

#define NUM_LEDS 1
#define LED_TYPE NEOPIXEL
#define DATA_PIN 3
#define CLOCK_PIN 13
#define COLOR_ORDER RGB

// This is an array of leds.  One item for each led in your strip.
CRGB leds[NUM_LEDS];

// This function sets up the ledsand tells the controller about them
void setup() {
  // sanity check delay - allows reprogramming if accidently blowing power w/leds
  delay(2000);

  // Uncomment this line for a 3-Wire chipset
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  // Uncomment this line for a SPI chipset
  //FastLED.addLeds<LED_TYPE, COLOR_ORDER>(leds, NUM_LEDS);

  // Uncomment this line for a SPI chipset with the data and clock pins specified
  //FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);
}

// This function runs over and over, and is where you do the magic to light
// your leds.
void loop() {
   // Move a single white led
   for(int whiteLed = 0; whiteLed < NUM_LEDS; whiteLed = whiteLed + 1) {
      // Turn our current led on to white, then show the leds
      leds[whiteLed] = CRGB::White;

      // Show the leds (only one of which is set to white, from above)
      FastLED.show();

      // Wait a little bit
      delay(100);

      // Turn our current led back to black for the next loop around
      leds[whiteLed] = CRGB::Black;
   }
}
