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

#define NUM_LEDS 160

struct CRGB { byte g; byte r; byte b; };

struct CRGB leds[NUM_LEDS];

// gdn clk data pwr
// Note: timing values in the code below are stale/out of date
// Hardware SPI Teensy 3 - .362ms for an 86 led frame 
// Hardware SPI - .652ms for an 86 led frame @8Mhz (3.1Mbps?), .913ms @4Mhz 1.434ms @2Mhz
// Hardware SPIr2 - .539ms @8Mhz, .799 @4Mhz, 1.315ms @2Mhz
// With the wait ordering reversed,  .520ms at 8Mhz, .779ms @4Mhz, 1.3ms @2Mhz
LPD8806Controller<11, 13, 10> LED;
// SM16716Controller<11, 13, 10> LED;

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
// WS2801Controller<11, 13, 10, 0> LED;

// Same Port, non-hardware SPI - 1.2ms for an 86 led frame, 1.12ms with large switch 
// WS2801Controller<11, 13, 10, 0> LED;

// Different Port, non-hardware SPI - 1.47ms for an 86 led frame
// WS2801Controller<7, 13, 10> LED;

// TM1809Controller800Mhz<6> LED;
// UCS1903Controller400Mhz<7> LED;
// WS2811Controller800Mhz<12> LED;
// WS2811Controller800Mhz<5> LED;
// TM1803Controller400Mhz<5> LED;

//////////////////////////////////////////////////////////////////////////////////////
//
// Sample code showing using multiple LED controllers simultaneously
//
// int ledset = 0;
// void show() { 
// 	switch(ledset) {
// 		case 0: LED.showRGB((byte*)leds, NUM_LEDS); break;
// 	 // 	case 1: LED2.showRGB((byte*)leds, NUM_LEDS); break;
// 		// case 2: LED3.showRGB((byte*)leds, NUM_LEDS); break;
// 		// case 3: LED4.showRGB((byte*)leds, NUM_LEDS); break;
// 		// case 4: LED5.showRGB((byte*)leds, NUM_LEDS); break;
// 	}	

// 	// LED.showRGB((byte*)leds, NUM_LEDS);
// 	// LED2.showRGB((byte*)leds, NUM_LEDS);
// 	// LED3.showRGB((byte*)leds, NUM_LEDS);
// 	// LED4.showRGB((byte*)leds, NUM_LEDS);
// 	// LED5.showRGB((byte*)leds, NUM_LEDS);
// 	memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB));
// }


void setup() {
#ifdef DEBUG
	delay(5000);

    Serial.begin(38400);
    Serial.println("resetting!");
#endif

	LED.init();

#ifdef DEBUG
	int start = millis();
	for(int i = 0; i < 1000; i++){ 
		LED.showRGB((byte*)leds, NUM_LEDS);
		// LED2.showRGB((byte*)leds, NUM_LEDS);
	}
	int end = millis();
	DPRINT("Time for 1000 loops: "); DPRINTLN(end - start);
#endif 
}

int count = 0;
long start = millis();

void loop() { 
#if 0
	memset(leds, 255, NUM_LEDS * sizeof(struct CRGB));
	LED.showRGB((byte*)leds, NUM_LEDS);
	delay(20);
#else
	for(int i = 0; i < 3; i++) {
		for(int iLed = 0; iLed < NUM_LEDS; iLed++) {
			memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB));
			switch(i) { 
			 	case 0: leds[iLed].r = 128; break;
			 	case 1: leds[iLed].g = 128; break;
			 	case 2: leds[iLed].b = 128; break;
			 }

	if(count == 0) { 
		start = millis();
	} 

	if(count++ == 1000) { 
		count = 0;
		DPRINT("Time for 1000 frames: "); DPRINTLN(millis() - start);
	}
			LED.showRGB((byte*)leds, NUM_LEDS);;
			//DPRINTLN("waiting");
//			delay(20);
		}
	}
	//  for(int i = 0; i < 64; i++) { 
	//  	memset(leds, i, NUM_LEDS * 3);
	// 	LED.showRGB((byte*)leds, NUM_LEDS);;
	// 	//	DPRINTLN("waiting");
	//  	delay(40);
	// }
	// for(int i = 64; i >= 0; i--) { 
	//  	memset(leds, i, NUM_LEDS * 3);
	// 	LED.showRGB((byte*)leds, NUM_LEDS);;
	// 	//	DPRINTLN("waiting");
	//  	delay(40);
	// }
#endif
}