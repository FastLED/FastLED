#include "FastLED.h"

// How many leds in your strip?
#define NUM_LEDS 64 

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806, define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 6
#define CLOCK_PIN 13

// Define the array of leds
CRGB leds[(NUM_LEDS * 16)+1];

void setup() { 
	Serial.begin(19200);
	delay(3000);
	Serial.println("resetting");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	delay(500); Serial.print(".");
	LEDS.addLeds<WS2811_PORTC,24>(leds, NUM_LEDS); // .setDither(DISABLE_DITHER);
	// LEDS.addLeds<NEOPIXEL,33>(leds,NUM_LEDS);
	Serial.println("Entering loopx");
}

void fadeall() { for(int i = 0; i < NUM_LEDS*16; i++) { leds[i].nscale8(250); } }

void loop() { 
	uint8_t hue = 0;
	// Serial.print("x");
	// First slide the led in one direction
	for(int i = 0; i < NUM_LEDS*5; i++) {
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

	// Now go in the other direction.  
	for(int i = (NUM_LEDS*5)-1; i >= 0; i--) {
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
