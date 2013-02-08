#include "FastSPI_LED2.h"

// #include "FastSPI_LED.h"
// #if defined(ARDUINO) && ARDUINO >= 100
//   #include "xxxArduino.h"
//   #include <xxxpins_arduino.h>
// #else
//   #include "xxxWProgram.h"
//   #include <xxxpins_arduino.h>
// #include "xxxwiring.h"
// #endif

#define DEBUG
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

// #include <FastSPI_LED2.h>
#define NUM_LEDS 86

struct CRGB { byte r; byte g; byte b; };

struct CRGB leds[NUM_LEDS];

// Hardware SPI - .652ms for an 86 led frame @8Mhz (3.1Mbps?), .913ms @4Mhz 1.434ms @2Mhz
// Hardware SPIr2 - .539ms @8Mhz, .799 @4Mhz, 1.315ms @2Mhz
// With the wait ordering reversed,  .520ms at 8Mhz, .779ms @4Mhz, 1.3ms @2Mhz
LPD8806Controller<11, 13, 10> LED;

// Same Port, non-hardware SPI - 1.2ms for an 86 led frame, 1.12ms with large switch 
// r2 - .939ms without large switch  .823ms with large switch
// r3 - .824ms removing 0 balancing nop, .823 with large switch removing balancing
// LPD8806Controller<12, 13, 10> LED;

// Different Port, non-hardware SPI - 1.47ms for an 86 led frame
// Different PortR2, non-hardware SPI - 1.07ms for an 86 led frame
// LPD8806Controller<7, 13, 10> LED;

// Hardware SPI - .652ms for an 86 led frame @8Mhz (3.1Mbps?), .913ms @4Mhz 1.434ms @2Mhz
// With the wait ordering reversed,  .520ms at 8Mhz, .779ms @4Mhz, 1.3ms @2Mhz
// WS2801Controller<11, 13, 10> LED;

// Same Port, non-hardware SPI - 1.2ms for an 86 led frame, 1.12ms with large switch 
// WS2801Controller<12, 13, 10> LED;

// Different Port, non-hardware SPI - 1.47ms for an 86 led frame
// WS2801Controller<7, 13, 10> LED;

// TM1809Controller800Mhz<6> LED;
// UCS1903Controller400Mhz<7> LED;
// WS2811Controller800Mhz<8> LED;
// TM1803Controller400Mhz<5> LED;

// struct aLED { void init() { FastSPI_LED.setLeds(NUM_LEDS, (unsigned char*)leds); FastSPI_LED.setPin(8); FastSPI_LED.setChipset(CFastSPI_LED::SPI_LPD8806); FastSPI_LED.init(); FastSPI_LED.start();}; void showRGB(byte *, int) { FastSPI_LED.show();} } LED;
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
    Serial.begin(38400);
    Serial.println("resetting!");
#endif

	LED.init();

#ifdef DEBUG
	int start = millis();
	for(int i = 0; i < 1000; i++){ 
		LED.showRGB((byte*)leds, NUM_LEDS);
	}
	int end = millis();
	DPRINT("Time for 1000 loops: "); DPRINTLN(end - start);
#endif 
}

void loop() { 
	memset(leds, 0, NUM_LEDS * sizeof(struct CRGB));
	LED.showRGB((byte*)leds, NUM_LEDS);
	// ledset++;
	// if(ledset > 4) { ledset = 0; }
	// ledset = 0;
	// // memset( leds, 128, NUM_LEDS * sizeof(struct CRGB));
	// // show();
	// for(int i = 0; i < 3; i++) {
	// 	for(int iLed = 0; iLed < NUM_LEDS; iLed++) {
	// 		memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB));
	// 		switch(i) { 
	// 		 	case 0: leds[iLed].r = 128; break;
	// 		 	case 1: leds[iLed].g = 128; break;
	// 		 	case 2: leds[iLed].b = 128; break;
	// 		 }

	// 		show();
	// 		// delay(20);
	// 	}
	// }
	// for(int i = 0; i < 64; i++) { 
	//  	memset(leds, i, NUM_LEDS * 3);
	//  	show();
	//  	delay(20);
	// }
	// for(int i = 64; i >= 0; i--) { 
	//  	memset(leds, i, NUM_LEDS * 3);
	//  	show();
	//  	delay(20);
	// }
}