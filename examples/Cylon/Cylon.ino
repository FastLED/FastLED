#include "FastLED.h"

#define NUM_LEDS 64
#define LED_TYPE NEOPIXEL
#define DATA_PIN 3
#define CLOCK_PIN 13
#define COLOR_ORDER RGB

#define BRIGHTNESS  84

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() {
	Serial.begin(57600);
	Serial.println("resetting");

	// Uncomment this line for a 3-Wire chipset
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  // Uncomment this line for a SPI chipset
  //FastLED.addLeds<LED_TYPE, COLOR_ORDER>(leds, NUM_LEDS);

  // Uncomment this line for a SPI chipset with the data and clock pins specified
  //FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);

	FastLED.setBrightness(BRIGHTNESS);
}

void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }

void loop() {
	static uint8_t hue = 0;
	Serial.print("x");
	// First slide the led in one direction
	for(int i = 0; i < NUM_LEDS; i++) {
		// Set the i'th led to red
		leds[i] = CHSV(hue++, 255, 255);
		// Show the leds
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		// leds[i] = CRGB::Black;
		fadeall();
		// Wait a little bit before we loop around and do it again
		delay(10);
	}
	Serial.print("x");

	// Now go in the other direction.
	for(int i = (NUM_LEDS)-1; i >= 0; i--) {
		// Set the i'th led to red
		leds[i] = CHSV(hue++, 255, 255);
		// Show the leds
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		// leds[i] = CRGB::Black;
		fadeall();
		// Wait a little bit before we loop around and do it again
		delay(10);
	}
}
