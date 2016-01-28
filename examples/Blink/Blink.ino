#include "FastLED.h"

#define NUM_LEDS 1
#define LED_TYPE NEOPIXEL
#define DATA_PIN 3
#define CLOCK_PIN 13
#define COLOR_ORDER RGB

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() {
  delay(2000); //Safety delay

  // Uncomment this line for a 3-Wire chipset
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  // Uncomment this line for a SPI chipset
  //FastLED.addLeds<LED_TYPE, COLOR_ORDER>(leds, NUM_LEDS);

  // Uncomment this line for a SPI chipset with the data and clock pins specified
  //FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);
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
