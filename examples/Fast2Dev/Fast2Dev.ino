// #define FAST_SPI_INTERRUPTS_WRITE_PINS 1
// #define FORCE_SOFTWARE_SPI 1
#include "FastSPI_LED2.h"

// #define DEBUG
#ifdef DEBUG
#define DPRINT Serial.print
#define DPRINTLN Serial.println
#else
#define DPRINT(x)
#define DPRINTLN(x)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// test code
//
//////////////////////////////////////////////////

#define NUM_LEDS 86

// struct CRGB { byte g; byte r; byte b; };

struct CRGB leds[NUM_LEDS];
struct CHSV hsv[NUM_LEDS];

// gdn clk data pwr
// Note: timing values in the code below are stale/out of date
// Hardware SPI Teensy 3 - .362ms for an 86 led frame 
// Hardware SPI - .652ms for an 86 led frame @8Mhz (3.1Mbps?), .913ms @4Mhz 1.434ms @2Mhz
// Hardware SPIr2 - .539ms @8Mhz, .799 @4Mhz, 1.315ms @2Mhz
// With the wait ordering reversed,  .520ms at 8Mhz, .779ms @4Mhz, 1.3ms @2Mhz
// LPD8806Controller<11, 13, NO_PIN, RBG, DATA_RATE_MHZ(24)> LED;
// LPD8806Controller<11, 13, NO_PIN, RBG, DATA_RATE_MHZ(1)> LEDSlow;

// SM16716Controller<11, 13, 10, RGB, 4> LED;

//LPD8806Controller<11, 13, 14> LED;
// LPD8806Controller<2, 1, 0> LED; // teensy pins
// LPD8806Controller<0, 4, 10> LED;
// LPD8806Controller<12, 13, 10, 0> LED;

// Same Port, non-hardware SPI - 1.2ms for an 86 led frame, 1.12ms with large switch 
// r2 - .939ms without large switch  .823ms with large switch
// r3 - .824ms removing 0 balancing nop, .823 with large switch removing balancing
// LPD8806Controller<12, 13, 10> LED;
// LPD8806Controller<14, 1, 10> LED; // teensy pins

// Different Port, non-hardware SPI - 1.47ms for an 86 led frame
// Different PortR2, non-hardware SPI - 1.07ms for an 86 led frame
// LPD8806Controller<7, 13, 10> LED;
// LPD8806Controller<8, 1, 10> LED;
// LPD8806Controller<11, 14, 10> LED;

// Hardware SPI - .652ms for an 86 led frame @8Mhz (3.1Mbps?), .913ms @4Mhz 1.434ms @2Mhz
// With the wait ordering reversed,  .520ms at 8Mhz, .779ms @4Mhz, 1.3ms @2Mhz
// WS2801Controller<11, 13, 10, RGB, 0> LED;

// Same Port, non-hardware SPI - 1.2ms for an 86 led frame, 1.12ms with large switch 
// WS2801Controller<11, 13, 10, RGB, 0> LED;

// Different Port, non-hardware SPI - 1.47ms for an 86 led frame
// WS2801Controller<7, 13, 10> LED;

// TM1809Controller800Khz<4, RGB> LED;
// UCS1903Controller400Khz<7> LED;
// WS2811Controller800Khz<12, BRG> LED;
WS2811Controller800Khz<23, GRB> LED;
// TM1803Controller400Khz<5> LED;

//////////////////////////////////////////////////////////////////////////////////////
//
// Sample code showing using multiple LED controllers simultaneously
//
// int ledset = 0;
// void show() { 
// 	switch(ledset) {
// 		case 0: LED.show((byte*)leds, NUM_LEDS); break;
// 	 // 	case 1: LED2.show((byte*)leds, NUM_LEDS); break;
// 		// case 2: LED3.show((byte*)leds, NUM_LEDS); break;
// 		// case 3: LED4.show((byte*)leds, NUM_LEDS); break;
// 		// case 4: LED5.show((byte*)leds, NUM_LEDS); break;
// 	}	

// 	// LED.show((byte*)leds, NUM_LEDS);
// 	// LED2.show((byte*)leds, NUM_LEDS);
// 	// LED3.show((byte*)leds, NUM_LEDS);
// 	// LED4.show((byte*)leds, NUM_LEDS);
// 	// LED5.show((byte*)leds, NUM_LEDS);
// 	memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB));
// }


static void volRun(CRGB *aled, int num) __attribute__((noinline));

static void volRun(CRGB *aled, int num) { 
	aled[num-1].r = 0;
}

void setup() {
#ifdef DEBUG
	delay(5000);

    Serial.begin(38400);
    Serial.println("resetting!");
#else
   	delay(2000);
#endif

    // LEDSlow.init();
    // LEDSlow.clearLeds(300);

    LED.init();

#ifdef DEBUG
    memset(hsv, 0, NUM_LEDS * sizeof(struct CHSV));

    unsigned long emptyStart = millis();
    for(volatile int i = 0 ; i < 10000; i++) {
      volRun(leds, NUM_LEDS);
    }
    unsigned long emptyEnd = millis();
	unsigned long start = millis();
	for(volatile int i = 0; i < 10000; i++){ 
		LED.show(leds, NUM_LEDS);
		// LED2.show((byte*)leds, NUM_LEDS);
	}
	unsigned long end = millis();
	DPRINT("Time for 10000 empty loops: "); DPRINTLN( emptyEnd - emptyStart);
	DPRINT("Time for 10000 loops: "); DPRINTLN(end - start);
	DPRINT("Time for 10000 adjusted loops: "); DPRINTLN( (end - start) - (emptyEnd - emptyStart));
#endif 
}

int count = 0;
long start = millis();

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
			LED.show(leds, NUM_LEDS);;
			delay(20);
		}

		LED.show(leds, NUM_LEDS);
		delay(2000);

		// fade up
		for(int i = 0; i < 128; i++) { 
			// The showColor method sets all the leds in the strip to the same color
			LED.showColor(CRGB(i, 0, 0), NUM_LEDS);
			delay(10);
		}

		// fade down
		for(int i = 128; i >= 0; i--) { 
			LED.showColor(CRGB(i, 0, 0), NUM_LEDS);
			delay(10);
		}

		// let's fade up by scaling the brightness
		for(int scale = 0; scale < 128; scale++) { 
			LED.showColor(CRGB(0, 128, 0), NUM_LEDS, scale);
			delay(10);
		}

		// let's fade down by scaling the brightness
		for(int scale = 128; scale > 0; scale--) { 
			LED.showColor(CRGB(0, 128, 0), NUM_LEDS, scale);
			delay(10);
		}
	}
}
