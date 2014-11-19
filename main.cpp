

#include "FastLED.h"

// How many leds in your strip?
#define NUM_LEDS 1

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 3
#define CLOCK_PIN 13

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() { 

  	  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void loop() { 
  fadeToBlackBy( leds, NUM_LEDS, 25);
  int i = random16(NUM_LEDS);
  leds[i] += CHSV( random8(), 255, 255);
  FastLED.show();
  delay(10);
}

int main(void) {
  setup();
  while(true) loop();
}


