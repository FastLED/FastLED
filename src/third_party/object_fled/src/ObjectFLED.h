/*  ObjectFLED - Teensy 4.x DMA to all pins for independent control of large and
	multiple LED display objects

	Copyright (c) 2024 Kurt Funderburg

	Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

	OctoWS2811 library code was well-studied and substantial portions of it used 
	to implement high-speed, non-blocking DMA for LED signal output in this library.
	See ObjectFLED.cpp for a summary of changes made to the original OctoWS2811.

	OctoWS2811 - High Performance WS2811 LED Display Library
	Copyright (c) 2013 Paul Stoffregen, PJRC.COM, LLC
	http://www.pjrc.com/teensy/td_libs_OctoWS2811.html

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#ifndef __IMXRT1062__
#error "Sorry, ObjectFLED only works on Teensy 4.x boards."
#endif
#if TEENSYDUINO < 121
#error "Teensyduino version 1.21 or later is required to compile this library."
#endif
#ifndef ObjectFLED_h
#define ObjectFLED_h
#include <WProgram.h>
#include "DMAChannel.h"

//Experimentally found DSE=3, SPEED=0 gave best LED overclocking
//boot defaults DSE=6, SPEED=2.
#define OUTPUT_PAD_DSE		3		//Legal values 0-7
#define OUTPUT_PAD_SPEED	0		//Legal values 0-3

// Ordinary RGB data is converted to GPIO bitmasks on-the-fly using
// a transmit buffer sized for 2 DMA transfers.  The larger this setting,
// the more interrupt latency OctoWS2811 can tolerate, but the transmit
// buffer grows in size.  For good performance, the buffer should be kept
// smaller than the half the Cortex-M7 data cache.
//bitdata[B_P_D * 64]		buffer holds data (10KB) for 80 LED bytes: 4DW * 8b = 32DW/LEDB = 96DW/LED
//framebuffer_index = B_P_D * 2 = pointer to next block for transfer (80 LEDB / bitdata buffer)
#define BYTES_PER_DMA	20		//= number of pairs of LEDB (40=80B) bitmasks in bitdata.

#define CORDER_RGB	0	//* WS2811, YF923
#define CORDER_RBG	1
#define CORDER_GRB	2	//* WS2811B, Most LED strips are wired this way
#define CORDER_GBR	3	//*
#define CORDER_BRG	4	//* Adafruit Product ID: 5984 As of November 5, 2024 - this strand has different 'internal' color ordering. It's now BRG not RGB,
#define CORDER_BGR	5	//* Adafruit Dotstar LEDs SK9822 uses this CO but they use inverted start/stop bits
#define CORDER_RGBW	6	//* Popular
#define CORDER_RBGW	7
#define CORDER_GRBW	8
#define CORDER_GBRW	9
#define CORDER_BRGW	10
#define CORDER_BGRW	11	// Adafruit Dotstar LEDs SK9822 uses this CO but they use inverted start/stop bits
#define CORDER_WRGB	12
#define CORDER_WRBG	13
#define CORDER_WGRB	14
#define CORDER_WGBR	15
#define CORDER_WBRG	16
#define CORDER_WBGR	17
#define CORDER_RWGB	18
#define CORDER_RWBG	19
#define CORDER_GWRB	20
#define CORDER_GWBR	21
#define CORDER_BWRG	22
#define CORDER_BWGR	23
#define CORDER_RGWB	24
#define CORDER_RBWG	25
#define CORDER_GRWB	26
#define CORDER_GBWR	27
#define CORDER_BRWG	28
#define CORDER_BGWR	29

namespace fl {


//Usage: ObjectFLED myCube ( Num_LEDs, *drawBuffer, LED_type, numPins, *pinList, serpentineNumber )
class ObjectFLED {
public:
	// Usage: ObjectFLED myCube ( Num_LEDs, *drawBuffer, LED_type, numPins, *pinList, serpentineNumber )
	// Example:
    // byte pinList[NUM_CHANNELS] = {1, 8, 14, 17, 24, 29, 20, 0, 15, 16, 18, 19, 21, 22, 23, 25};
    // byte pinListBlank[7] = {1, 8, 14, 17, 24, 29, 20};
    // CRGB testCube[NUM_PLANES][NUM_ROWS][PIX_PER_ROW];
    // CRGB blankLeds[7][8][8];
    // ObjectFLED leds(PIX_PER_ROW * NUM_ROWS * NUM_PLANES, testCube, CORDER_RGB, NUM_CHANNELS, pinList);
	// void setup() {
	//    leds.begin(1.6, 72);    //1.6 ocervlock factor, 72uS LED latch delay
	//    leds.setBrightness(64);
	// }
	// void loop() {
	//    leds.show();
	//    delay(100);
	// }
	ObjectFLED(uint16_t numLEDs, void* drawBuf, uint8_t config, uint8_t numPins, const uint8_t* pinList, \
				uint8_t serpentine = 0);

	~ObjectFLED() { 
		// Wait for prior xmission to end, don't need to wait for latch time before deleting buffer
		while (micros() - update_begin_micros < numbytes * 8 * TH_TL / OC_FACTOR / 1000 + 5);
		delete frameBuffer; 
	}

	//begin() - Use defalut LED timing: 1.0 OC Factor, 1250 nS CLK (=800 KHz), 300 nS T0H, 750 nS T1H, 300 uS LED Latch Delay.
	void begin(void);

	//begin(LED_Latch_Delay_uS) - sets the LED Latch Delay.
	void begin(uint16_t);

	//begin(LED_Overclock_Factor, LED_Latch_Delay_uS) - divides default 1250 nS LED CLK (=800 KHz), 
	// 300 nS T0H, 750 nS T1H; and optionally sets the LED Latch Delay.
	void begin(double, uint16_t = 300);

	//begin(LED_CLK_nS, LED_T0H_nS, LED_T1H_nS, LED_Latch_Delay_uS) - specifies full LED waveform timing.
	void begin(uint16_t, uint16_t, uint16_t, uint16_t = 300);

	void show(void);
	void waitForDmaToFinish() {
		while (!dma3.complete()) {  // wait for dma to complete before reset/re-use
			delayMicroseconds(10);
		}
	}
	
	int busy(void);

	//Brightness values 0-255
	void setBrightness(uint8_t);

	//Color Balance is 3-byte number in RGB order.  Each byte is a brightness value for that color.
	void setBalance(uint32_t);

	uint8_t getBrightness() { return brightness; }

	uint32_t getBalance() { return colorBalance; }

private:
	static void isr(void);

	void genFrameBuffer(uint32_t);

	static uint8_t* frameBuffer;					//isr()
	static uint32_t numbytes;						//isr()
	static uint8_t numpins;							//isr()
	static uint8_t pin_bitnum[NUM_DIGITAL_PINS];	//isr()
	static uint8_t pin_offset[NUM_DIGITAL_PINS];	//isr()
	DMAMEM static uint32_t bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));	//isr()
	DMAMEM static uint32_t bitmask[4] __attribute__((used, aligned(32)));
	static DMAChannel dma1, dma2, dma3;
	static DMASetting dma2next;

	uint32_t update_begin_micros = 0;
	uint8_t brightness = 255;
	uint32_t colorBalance = 0xFFFFFF;
	uint32_t rLevel = 65025;
	uint32_t gLevel = 65025;
	uint32_t bLevel = 65025;
	void* drawBuffer;
	uint16_t stripLen;
	uint8_t params;
	uint8_t pinlist[NUM_DIGITAL_PINS];
	uint16_t comp1load[3];
	uint8_t serpNumber;
	float OC_FACTOR = 1.0;			//used to reduce period of LED output
	uint16_t TH_TL = 1250;			//nS- period of LED output
	uint16_t T0H = 300;				//nS- duration of T0H
	uint16_t T1H = 750;				//nS- duration of T1H
	uint16_t LATCH_DELAY = 300;		//uS time to hold output low for LED latch.

	//for show context switch
	uint32_t bitmaskLocal[4];
	uint8_t numpinsLocal;
	uint8_t* frameBufferLocal;
	uint32_t numbytesLocal;
	uint8_t pin_bitnumLocal[NUM_DIGITAL_PINS];
	uint8_t pin_offsetLocal[NUM_DIGITAL_PINS];
};	// class ObjectFLED


//fadeToColorBy(RGB_array, LED_count, Color, FadeAmount)
//Fades an RGB array towards the background color by amount. 
void fadeToColorBy(void*, uint16_t, uint32_t, uint8_t);


//drawSquare(RGB_Array, LED_Rows, LED_Cols, Y_Corner, X_Corner, square_Size)
//Draws square in a 2D RGB array with lower left corner at (Y_Corner, X_Corner).
//Safe to specify -Y, -X corner, safe to draw a box which partially fits on LED plane.
void drawSquare(void*, uint16_t, uint16_t, int, int, uint32_t, uint32_t);

}	// namespace fl

#endif
