#include "FastLED.h"

#define NUM_LEDS 6
#define LED_TYPE NEOPIXEL
#define DATA_PIN 6
#define CLOCK_PIN 13
#define COLOR_ORDER RGB

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RGB Calibration code
//
// Use this sketch to determine what the RGB ordering for your chipset should be.  Steps for setting up to use:

// * Uncomment the line in setup that corresponds to the LED chipset that you are using.  (Note that they
//   all explicitly specify the RGB order as RGB)
// * Define DATA_PIN to the pin that data is connected to.
// * (Optional) if using software SPI for chipsets that are SPI based, define CLOCK_PIN to the clock pin
// * Compile/upload/run the sketch

// You should see six leds on.  If the RGB ordering is correct, you should see 1 red led, 2 green
// leds, and 3 blue leds.  If you see different colors, the count of each color tells you what the
// position for that color in the rgb orering should be.  So, for example, if you see 1 Blue, and 2
// Red, and 3 Green leds then the rgb ordering should be BRG (Blue, Red, Green).

// You can then test this ordering by setting the RGB ordering in the addLeds line below to the new ordering
// and it should come out correctly, 1 red, 2 green, and 3 blue.
//
//////////////////////////////////////////////////


CRGB leds[NUM_LEDS];

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

void loop() {
   leds[0] = CRGB(255,0,0);
   leds[1] = CRGB(0,255,0);
   leds[2] = CRGB(0,255,0);
   leds[3] = CRGB(0,0,255);
   leds[4] = CRGB(0,0,255);
   leds[5] = CRGB(0,0,255);
   leds[random8()%NUM_LEDS] = CRGB(0,0,0);
   // leds[10] = CRGB(0,0,0);
   FastLED.show();
   // delay(1000);
   FastLED.showColor(CRGB::Black);
}
