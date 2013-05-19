// Uncomment this line if you have any interrupts that are changing pins - this causes the library to be a little bit more cautious
// #define FAST_SPI_INTERRUPTS_WRITE_PINS 1

// Uncomment this line to force always using software, instead of hardware, SPI (why?)
// #define FORCE_SOFTWARE_SPI 1

#include "FastSPI_LED2.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// test code
//
//////////////////////////////////////////////////

#define NUM_LEDS 20 // 86

struct CRGB leds[NUM_LEDS];

void setup() {
	// sanity check delay - allows reprogramming if accidently blowing power w/leds
   	delay(2000);

   	LEDS.addLeds<LPD8806>(leds, NUM_LEDS);
	// LEDS.addLeds<WS2811, 12>(leds, NUM_LEDS);

	// Put ws2801 strip on the hardware SPI pins with a BGR ordering of rgb and limited to a 1Mhz data rate
	// LEDS.addLeds<WS2801, 11, 13, BGR, DATA_RATE_MHZ(1)>(leds, NUM_LEDS);

   	// LEDS.addLeds<LPD8806, 10, 11>(leds, NUM_LEDS);
   	// LEDS.addLeds<WS2811, 13, BRG>(leds, NUM_LEDS);
   	// LEDS.addLeds<LPD8806, BGR>(leds, NUM_LEDS);
}

void loop() { 
	for(int i = 0; i < 3; i++) {
		for(int iLed = 0; iLed < NUM_LEDS; iLed++) {
			memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB));

			switch(i) { 
				// You can access the rgb values by field r, g, b
			 	case 0: leds[iLed].r = 128; break;

			 	// or by indexing into the led (r==0, g==1, b==2) 
			 	case 1: leds[iLed][i] = 128; break;

			 	// or by setting the rgb values for the pixel all at once
			 	case 2: leds[iLed] = CRGB(0, 0, 128); break;
			}

			// and now, show your led array! 
			LEDS.show();
			delay(10); //00);
		}

		//delay(2000);
		// LED.Sshow();
		// delay(2000);

		// fade up
		for(int i = 0; i < 128; i++) { 
			// The showColor method sets all the leds in the strip to the same color
			LEDS.showColor(CRGB(i, 0, 0));
			delay(10);
		}

		// fade down
		for(int i = 128; i >= 0; i--) { 
			LEDS.showColor(CRGB(i, 0, 0));
			delay(10);
		}

		// let's fade up by scaling the brightness
		for(int scale = 0; scale < 128; scale++) { 
			LEDS.showColor(CRGB(0, 128, 0), scale);
			delay(10);
		}

		// let's fade down by scaling the brightness
		for(int scale = 128; scale > 0; scale--) { 
			LEDS.showColor(CRGB(0, 128, 0), scale);
			delay(10);
		}
	}
}
