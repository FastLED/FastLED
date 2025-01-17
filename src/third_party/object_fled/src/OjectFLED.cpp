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
	See below for a summary of changes made to the original OctoWS2811.

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
/* 
Teensy 4.0 pin - port assignments
GPIO6List = { 0, 1, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 } //12 top, 4 bottom
GPIO7List = { 6, 7, 8, 9, 10, 11, 12, 13, 32 }  //8 top, 1 bottom
GPIO8List = { 28, 30, 31, 34, 35, 36, 37, 38, 39 }  //0 top, 9 bottom
GOIO9List = { 2, 3, 4, 5, 29, 33 }  //4 top, 2 bottom
Teensy 4.1 pin - port assignments
GPIO6List = { 0, 1, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 38, 39, 40, 41 } //20 top, 0 bottom
GPIO7List = { 6, 7, 8, 9, 10, 11, 12, 13, 32, 34, 35, 36, 37 }  //13 top, 0 bottom
GPIO8List = { 28, 30, 31, 42, 43, 44, 45, 46, 47 }  //3 top, 6 bottom
GOIO9List = { 2, 3, 4, 5, 29, 33, 48, 49, 50, 51, 52, 53, 54 }  //6 top, 7 bottom
* 4 pin groups, 4 timer groups, !4 dma channel groups: only 1 DMA group (4 ch) maps to GPIO (DMAMUX
* mapping) Also, only DMA ch0-3 have periodic mode for timer trigger (p77 manual). Separate objects 
* cannot DMA at once.
* 
* CHANGES:
* Moved some variables so class supports multiple instances with separate LED config params
* Implemented context switching so multiple instances can show() independently
* Re-parameterized TH_TL, T0H, T1H, OC_FACTOR; recalc time for latch at end of show()
* Added genFrameBuffer() to implement RGB order, brightness, color balance, and serpentine
* Added setBrightness(), setBalance()
* FrameBuffer no longer passed in, constructor now creates buffer; destructor added
* Added support for per-object setting of OC factor, TH+TL, T0H, T1H, and LATCH_DELAY in begin function
* Set DSE=3, SPEED=0, SRE=0 on output pins per experiment & PJRC forum guidance
* New default values for TH_TL, T0H, T1H, LATCH_DELAY to work with Audio lib and more LED types
* Added wait for prior xmission to complete in destructor
*/

#ifndef __IMXRT1062__
// Do nothing for other platforms.
#else
#include "ObjectFLED.h"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

namespace fl {

volatile uint32_t framebuffer_index = 0;		//isr()
uint8_t* ObjectFLED::frameBuffer;				//isr()
uint32_t ObjectFLED::numbytes;					//isr()
uint8_t ObjectFLED::numpins;					//isr()
uint8_t ObjectFLED::pin_bitnum[NUM_DIGITAL_PINS];	//isr()
uint8_t ObjectFLED::pin_offset[NUM_DIGITAL_PINS];	//isr()
uint32_t ObjectFLED::bitdata[BYTES_PER_DMA * 64] __attribute__((used, aligned(32)));	//isr()
uint32_t ObjectFLED::bitmask[4] __attribute__((used, aligned(32)));

DMASetting ObjectFLED::dma2next;
DMAChannel ObjectFLED::dma1;
DMAChannel ObjectFLED::dma2;
DMAChannel ObjectFLED::dma3;
volatile bool dma_first;


ObjectFLED::ObjectFLED(uint16_t numLEDs, void *drawBuf, uint8_t config, uint8_t numPins, \
					const uint8_t *pinList, uint8_t serpentine) {
	serpNumber = serpentine;
	drawBuffer = drawBuf;
	params = config;
	if (numPins > NUM_DIGITAL_PINS) numPins = NUM_DIGITAL_PINS;
	numpins = numPins;			//static/isr
	stripLen = numLEDs / numpins;
	memcpy(pinlist, pinList, numpins);
	if ((params & 0x3F) < 6) {
		frameBuffer = new uint8_t[numLEDs * 3];		//static/isr
		numbytes = stripLen * 3; // RGB formats	//static/isr
	}
	else {
		frameBuffer = new uint8_t[numLEDs * 4];		//static/isr
		numbytes = stripLen * 4; // RGBW formats	//static/isr
	}

	numpinsLocal = numPins;
	frameBufferLocal = frameBuffer;
	numbytesLocal = numbytes;
}	// ObjectFLED constructor


extern "C" void xbar_connect(unsigned int input, unsigned int output); // in pwm.c
static volatile uint32_t *standard_gpio_addr(volatile uint32_t *fastgpio) {
	return (volatile uint32_t *)((uint32_t)fastgpio - 0x01E48000);
}


void ObjectFLED::begin(uint16_t latchDelay) {
	LATCH_DELAY = latchDelay;
	begin();
}


void ObjectFLED::begin(double OCF, uint16_t latchDelay) {
	OC_FACTOR = (float)OCF;
	LATCH_DELAY = latchDelay;
	begin();
}


void ObjectFLED::begin(uint16_t period, uint16_t t0h, uint16_t t1h, uint16_t latchDelay) {
	TH_TL = period;
	T0H = t0h;
	T1H = t1h;
	LATCH_DELAY = latchDelay;
	begin();
}


// INPUT stripLen, frameBuffer, params, numPins, pinList
// GPIOR bits set for pins[i] -> bitmask, pin_bitnum[i], pin_offset[i]
// init timers, xbar to DMA, DMA bitdata -> GPIOR; clears frameBuffer (total LEDs * 3 bytes)
void ObjectFLED::begin(void) {
	numpins = numpinsLocal;		//needed to compute pin mask/offset & bitmask since static for isr
	// Set each pin's bitmask bit, store offset & bit# for pin
	memset(bitmask, 0, sizeof(bitmask));
	for (uint32_t i=0; i < numpins; i++) {
		uint8_t pin = pinlist[i];
		if (pin >= NUM_DIGITAL_PINS) continue;	// ignore illegal pins
		uint8_t bit = digitalPinToBit(pin);		// pin's bit index in word port DR
		// which GPIO R controls this pin: 0-3 map to GPIO6-9 then map to DMA compat GPIO1-4
		uint8_t offset = ((uint32_t)portOutputRegister(pin) - (uint32_t)&GPIO6_DR) >> 14;
		if (offset > 3) continue;	//ignore unknown pins
		pin_bitnum[i] = bit;		//static/isr
		pin_offset[i] = offset;		//static/isr
		uint32_t mask = 1 << bit;	//mask32 = bit set @position in GPIO DR
		bitmask[offset] |= mask;	//bitmask32[0..3] = collective pin bit masks for each GPIO DR
		//bit7:6 SPEED; bit 5:3 DSE; bit0 SRE  (default SPEED = 0b10; def. DSE = 0b110)
		*portControlRegister(pin) &= ~0xF9;		//clear SPEED, DSE, SRE
		*portControlRegister(pin) |= ((OUTPUT_PAD_SPEED & 0x3) << 6) | \
			((OUTPUT_PAD_DSE & 0x7) << 3);	//DSE = 0b011 for LED overclock
		//clear pin bit in IOMUX_GPR26 to map GPIO6-9 to GPIO1-4 for DMA 
		*(&IOMUXC_GPR_GPR26 + offset) &= ~mask;		
		*standard_gpio_addr(portModeRegister(pin)) |= mask;		//GDIR? bit flag set output mode
	}
	//stash context for multi-show
	memcpy(bitmaskLocal, bitmask, 16);
	memcpy(pin_bitnumLocal, pin_bitnum, numpins);
	memcpy(pin_offsetLocal, pin_offset, numpins);

	arm_dcache_flush_delete(bitmask, sizeof(bitmask));			//can't DMA from cached memory

	// Set up 3 timers to create waveform timing events
	comp1load[0] = (uint16_t)((float)F_BUS_ACTUAL / 1000000000.0 * (float)TH_TL / OC_FACTOR );
	comp1load[1] = (uint16_t)((float)F_BUS_ACTUAL / 1000000000.0 * (float)T0H / OC_FACTOR );
	comp1load[2] = (uint16_t)((float)F_BUS_ACTUAL / 1000000000.0 * (float)T1H / (1.0 + ((OC_FACTOR - 1.0)/3)) );
	TMR4_ENBL &= ~7;
	TMR4_SCTRL0 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE | TMR_SCTRL_MSTR;
	TMR4_CSCTRL0 = TMR_CSCTRL_CL1(1) | TMR_CSCTRL_TCF1EN;
	TMR4_CNTR0 = 0;
	TMR4_LOAD0 = 0;
	TMR4_COMP10 = comp1load[0];
	TMR4_CMPLD10 = comp1load[0];
	TMR4_CTRL0 = TMR_CTRL_CM(1) | TMR_CTRL_PCS(8) | TMR_CTRL_LENGTH | TMR_CTRL_OUTMODE(3);
	TMR4_SCTRL1 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE;
	TMR4_CNTR1 = 0;
	TMR4_LOAD1 = 0;
	TMR4_COMP11 = comp1load[1]; // T0H
	TMR4_CMPLD11 = comp1load[1];
	TMR4_CTRL1 = TMR_CTRL_CM(1) | TMR_CTRL_PCS(8) | TMR_CTRL_COINIT | TMR_CTRL_OUTMODE(3);
	TMR4_SCTRL2 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE;
	TMR4_CNTR2 = 0;
	TMR4_LOAD2 = 0;
	TMR4_COMP12 = comp1load[2]; // T1H
	TMR4_CMPLD12 = comp1load[2];
	TMR4_CTRL2 = TMR_CTRL_CM(1) | TMR_CTRL_PCS(8) | TMR_CTRL_COINIT | TMR_CTRL_OUTMODE(3);

	// route the timer outputs through XBAR to edge trigger DMA request: only 4 mappings avail.
	CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON);
	xbar_connect(XBARA1_IN_QTIMER4_TIMER0, XBARA1_OUT_DMA_CH_MUX_REQ30);	
	xbar_connect(XBARA1_IN_QTIMER4_TIMER1, XBARA1_OUT_DMA_CH_MUX_REQ31);
	xbar_connect(XBARA1_IN_QTIMER4_TIMER2, XBARA1_OUT_DMA_CH_MUX_REQ94);
	XBARA1_CTRL0 = XBARA_CTRL_STS1 | XBARA_CTRL_EDGE1(3) | XBARA_CTRL_DEN1 |
					XBARA_CTRL_STS0 | XBARA_CTRL_EDGE0(3) | XBARA_CTRL_DEN0;
	XBARA1_CTRL1 = XBARA_CTRL_STS0 | XBARA_CTRL_EDGE0(3) | XBARA_CTRL_DEN0;

	// configure DMA channels
	dma1.begin();
	dma1.TCD->SADDR = bitmask;							// source 4*32b GPIO pin mask
	dma1.TCD->SOFF = 8;									// bytes offset added to SADDR after each transfer
	// SMOD(4) low bits of SADDR to update with adds of SOFF
	// SSIZE(3) code for 64 bit transfer size  DSIZE(2) code for 32 bit transfer size
	dma1.TCD->ATTR = DMA_TCD_ATTR_SSIZE(3) | DMA_TCD_ATTR_SMOD(4) | DMA_TCD_ATTR_DSIZE(2);
	dma1.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_DMLOE |	// Dest minor loop offsetting enable
		DMA_TCD_NBYTES_MLOFFYES_MLOFF(-65536) |
		DMA_TCD_NBYTES_MLOFFYES_NBYTES(16);				// #bytes to tansfer, offsetting enabled
	dma1.TCD->SLAST = 0;								// add to SADDR after xfer
	dma1.TCD->DADDR = &GPIO1_DR_SET;
	dma1.TCD->DOFF = 16384;								//&GPIO1_DR_SET + DOFF = next &GPIO2_DR_SET
	dma1.TCD->CITER_ELINKNO = numbytes * 8;				// CITER outer loop count (linking disabled) = # LED bits to write
	dma1.TCD->DLASTSGA = -65536;						// add to DADDR after xfer
	dma1.TCD->BITER_ELINKNO = numbytes * 8;				// Beginning CITER (not decremented by transfer)
	dma1.TCD->CSR = DMA_TCD_CSR_DREQ;					// channel ERQ field cleared when minor loop completed
	dma1.triggerAtHardwareEvent(DMAMUX_SOURCE_XBAR1_0);	// only 4 XBAR1 triggers (DMA MUX mapping)

	dma2next.TCD->SADDR = bitdata;						//uint32_t bitdata[BYTES_PER_DMA*64]
	dma2next.TCD->SOFF = 8;
	dma2next.TCD->ATTR = DMA_TCD_ATTR_SSIZE(3) | DMA_TCD_ATTR_DSIZE(2);
	dma2next.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_DMLOE |
		DMA_TCD_NBYTES_MLOFFYES_MLOFF(-65536) |
		DMA_TCD_NBYTES_MLOFFYES_NBYTES(16);
	dma2next.TCD->SLAST = 0;
	dma2next.TCD->DADDR = &GPIO1_DR_CLEAR;
	dma2next.TCD->DOFF = 16384;
	dma2next.TCD->CITER_ELINKNO = BYTES_PER_DMA * 8;
	dma2next.TCD->DLASTSGA = (int32_t)(dma2next.TCD);
	dma2next.TCD->BITER_ELINKNO = BYTES_PER_DMA * 8;
	dma2next.TCD->CSR = 0;

	dma2.begin();
	dma2 = dma2next; // copies TCD
	dma2.triggerAtHardwareEvent(DMAMUX_SOURCE_XBAR1_1);
	dma2.attachInterrupt(isr);

	dma3.begin();
	dma3.TCD->SADDR = bitmask;
	dma3.TCD->SOFF = 8;
	dma3.TCD->ATTR = DMA_TCD_ATTR_SSIZE(3) | DMA_TCD_ATTR_SMOD(4) | DMA_TCD_ATTR_DSIZE(2);
	dma3.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_DMLOE |
		DMA_TCD_NBYTES_MLOFFYES_MLOFF(-65536) |
		DMA_TCD_NBYTES_MLOFFYES_NBYTES(16);
	dma3.TCD->SLAST = 0;
	dma3.TCD->DADDR = &GPIO1_DR_CLEAR;
	dma3.TCD->DOFF = 16384;
	dma3.TCD->CITER_ELINKNO = numbytes * 8;
	dma3.TCD->DLASTSGA = -65536;
	dma3.TCD->BITER_ELINKNO = numbytes * 8;
	dma3.TCD->CSR = DMA_TCD_CSR_DREQ | DMA_TCD_CSR_DONE;
	dma3.triggerAtHardwareEvent(DMAMUX_SOURCE_XBAR1_2);
}	// begin()


//*dest = *bitdata + pin offset
//*pixels = pin's block in frameBuffer
//mask = pin's bit position in GPIOR
//set a pin's mask32 for each color bit=0 at every 4*words32 in bitdata+offset
void fillbits(uint32_t *dest, const uint8_t *pixels, int n, uint32_t mask) {
	do {
		uint8_t pix = *pixels++; 
		if (!(pix & 0x80)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x40)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x20)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x10)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x08)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x04)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x02)) *dest |= mask;
		dest += 4;
		if (!(pix & 0x01)) *dest |= mask;
		dest += 4;
	} while (--n > 0);
}


void ObjectFLED::genFrameBuffer(uint32_t serp) {
	uint32_t j = 0;
	int jChange = -3;
	if (serp == 0) {	// use faster loops if no serp
		switch (params & 0x3F) {
		case CORDER_RGBW:		// R,G,B = R,G,B - MIN(R,G,B); W = MIN(R,G,B)
			for (uint16_t i = 0; i < (numbytes * numpins); i += 4) {
				uint8_t minRGB = MIN(*((uint8_t*)drawBuffer + j) * rLevel / 65025, \
					*((uint8_t*)drawBuffer + j + 1) * rLevel / 65025);
				minRGB = MIN(minRGB, *((uint8_t*)drawBuffer + j + 2) * rLevel / 65025);
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j) * rLevel / 65025 - minRGB;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025 - minRGB;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025 - minRGB;
				*(frameBuffer + i + 3) = minRGB;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_GBR:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_BGR:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_BRG:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_GRB:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_RGB:
		default:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
		}	// switch()
	} else {	//serpentine
		switch (params & 0x3F) {
		case CORDER_RGBW:		// R,G,B = R,G,B - MIN(R,G,B); W = MIN(R,G,B)
			for (uint16_t i = 0; i < (numbytes * numpins); i += 4) {
				uint8_t minRGB = MIN(*((uint8_t*)drawBuffer + j) * rLevel / 65025, \
					* ((uint8_t*)drawBuffer + j + 1) * rLevel / 65025);
				minRGB = MIN(minRGB, *((uint8_t*)drawBuffer + j + 2) * rLevel / 65025);
				if (i % (serp * 4) == 0) {
					if (jChange < 0) { j = i / 4 * 3; jChange = 3; }
					else { j = (i / 4 + serp - 1) * 3; jChange = -3; }
				}
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j) * rLevel / 65025 - minRGB;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025 - minRGB;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025 - minRGB;
				*(frameBuffer + i + 3) = minRGB;
				j += jChange;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_GBR:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				if (i % (serp * 3) == 0) {
					if (jChange < 0) { j = i; jChange = 3; }
					else { j = i + (serp - 1) * 3; jChange = -3; }
				}
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_BGR:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				if (i % (serp * 3) == 0) {
					if (jChange < 0) { j = i; jChange = 3; }
					else { j = i + (serp - 1) * 3; jChange = -3; }
				}
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += 3;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_BRG:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				if (i % (serp * 3) == 0) {
					if (jChange < 0) { j = i; jChange = 3; }
					else { j = i + (serp - 1) * 3; jChange = -3; }
				}
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += jChange;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_GRB:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				if (i % (serp * 3) == 0) {
					if (jChange < 0) { j = i; jChange = 3; }
					else { j = i + (serp - 1) * 3; jChange = -3; }
				}
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += jChange;
			}	//for(leds in drawbuffer)
			break;
		case CORDER_RGB:
		default:
			for (uint16_t i = 0; i < (numbytes * numpins); i += 3) {
				if (i % (serp * 3) == 0) {
					if (jChange < 0) { j = i; jChange = 3; }
					else { j = i + (serp - 1) * 3; jChange = -3; }
				}
				*(frameBuffer + i) = *((uint8_t*)drawBuffer + j) * rLevel / 65025;
				*(frameBuffer + i + 1) = *((uint8_t*)drawBuffer + j + 1) * gLevel / 65025;
				*(frameBuffer + i + 2) = *((uint8_t*)drawBuffer + j + 2) * bLevel / 65025;
				j += jChange;
			}	//for(leds in drawbuffer)
		}	// switch()
	} // else serpentine
}	//genFrameBuffer()


// pre-show prior transfer wait, copies drawBuffer -> frameBuffer
// resets timers, clears pending DMA reqs
// fills bitdata[BYTES_PER_DMA * 64 * 4 bytes] from frameBuffer with 4-block bitmasks for 0's in led data
// 4 word32s for each bit in (led data)/pin = 16 * 8 = 96 bitdata bytes for each LED byte: 288 bytes / LED
// launches DMA with IRQ activation to reload bitdata from frameBuffer
void ObjectFLED::show(void) {
	waitForDmaToFinish();	//wait for prior DMA to finish

	//Restore context if needed
	if (frameBuffer != frameBufferLocal) {		
		numpins = numpinsLocal;
		frameBuffer = frameBufferLocal;
		numbytes = numbytesLocal;
		memcpy(bitmask, bitmaskLocal, 16);
		memcpy(pin_bitnum, pin_bitnumLocal, numpins);
		memcpy(pin_offset, pin_offsetLocal, numpins);
		arm_dcache_flush_delete(bitmask, sizeof(bitmask));			//can't DMA from cached memory
		// Restore 3 timers to create waveform timing events
		TMR4_COMP10 = comp1load[0];
		TMR4_CMPLD10 = comp1load[0];
		TMR4_COMP11 = comp1load[1]; // T0H
		TMR4_CMPLD11 = comp1load[1];
		TMR4_COMP12 = comp1load[2]; // T1H
		TMR4_CMPLD12 = comp1load[2];
		//restore DMA loop control
		dma1.TCD->CITER_ELINKNO = numbytes * 8;		// CITER outer loop count (linking disabled) = # LED bits to write
		dma1.TCD->BITER_ELINKNO = numbytes * 8;		// Beginning CITER (not decremented by transfer)
		dma3.TCD->CITER_ELINKNO = numbytes * 8;
		dma3.TCD->BITER_ELINKNO = numbytes * 8;
	} //done restoring context

	genFrameBuffer(serpNumber);

	// disable timers
	uint16_t enable = TMR4_ENBL;
	TMR4_ENBL = enable & ~7;

	// force all timer outputs to logic low
	TMR4_SCTRL0 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE | TMR_SCTRL_MSTR;
	TMR4_SCTRL1 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE;
	TMR4_SCTRL2 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE;

	// clear any prior pending DMA requests
	XBARA1_CTRL0 |= XBARA_CTRL_STS1 | XBARA_CTRL_STS0;
	XBARA1_CTRL1 |= XBARA_CTRL_STS0;

	// fill the DMA transmit buffer
	memset(bitdata, 0, sizeof(bitdata));			//BYTES_PER_DMA * 64 words32
	uint32_t count = numbytes;						//bytes per strip
	if (count > BYTES_PER_DMA*2) count = BYTES_PER_DMA*2;
	framebuffer_index = count;						//ptr to framebuffer last byte output

	//Sets each pin mask in bitdata32[BYTES_PER_DMA*64] for every 0 bit of pin's frameBuffer block bytes
	for (uint32_t i=0; i < numpins; i++) {			//for each pin
		fillbits(bitdata + pin_offset[i], (uint8_t *)frameBuffer + i*numbytes,
			count, 1<<pin_bitnum[i]);
	}
	arm_dcache_flush_delete(bitdata, count * 128);	// don't need bitdata in cache for DMA

    // set up DMA transfers
    if (numbytes <= BYTES_PER_DMA*2) {
		dma2.TCD->SADDR = bitdata;
		dma2.TCD->DADDR = &GPIO1_DR_CLEAR;
		dma2.TCD->CITER_ELINKNO = count * 8;
		dma2.TCD->CSR = DMA_TCD_CSR_DREQ;
    } else {
		dma2.TCD->SADDR = bitdata;
		dma2.TCD->DADDR = &GPIO1_DR_CLEAR;
		dma2.TCD->CITER_ELINKNO = BYTES_PER_DMA * 8;
		dma2.TCD->CSR = 0;
		dma2.TCD->CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_ESG;
		dma2next.TCD->SADDR = bitdata + BYTES_PER_DMA*32;
		dma2next.TCD->CITER_ELINKNO = BYTES_PER_DMA * 8;
		if (numbytes <= BYTES_PER_DMA*3) {
			dma2next.TCD->CSR = DMA_TCD_CSR_ESG;
		} else {
			dma2next.TCD->CSR = DMA_TCD_CSR_ESG | DMA_TCD_CSR_INTMAJOR;
		}
		dma_first = true;
    }
	dma3.clearComplete();
	dma1.enable(); 
	dma2.enable();
	dma3.enable();

	// initialize timers
	TMR4_CNTR0 = 0;
	TMR4_CNTR1 = comp1load[0] + 1;
	TMR4_CNTR2 = comp1load[0] + 1;

	// wait for last LED reset to finish
	while (micros() - update_begin_micros < numbytes * 8 * TH_TL / OC_FACTOR / 1000 + LATCH_DELAY);

	// start everything running!
	TMR4_ENBL = enable | 7;
	update_begin_micros = micros();
}	// show()


//INPUT: dma2, dma2next, bitdata, framebuffer_inedex, numpins, numbytes, pin_offset[], pin_bitnum[]
//Reads next block of framebuffer -> fillbits() -> bitdata
//Checks for last block to transfer, next to last, or not to update dma2next major loop
void ObjectFLED::isr(void)
{
	// first ack the interrupt
	dma2.clearInterrupt();

	// fill (up to) half the transmit buffer with new fillbits(frameBuffer data)
	//digitalWriteFast(12, HIGH);
	uint32_t *dest;
	if (dma_first) {
		dma_first = false;
		dest = bitdata;
	} else {
		dma_first = true;
		dest = bitdata + BYTES_PER_DMA*32;
	}
	memset(dest, 0, sizeof(bitdata)/2);
	uint32_t index = framebuffer_index;
	uint32_t count = numbytes - framebuffer_index;
	if (count > BYTES_PER_DMA) count = BYTES_PER_DMA;
	framebuffer_index = index + count;
	for (int i=0; i < numpins; i++) {
		fillbits(dest + pin_offset[i], (uint8_t *)frameBuffer + index + i*numbytes,
			count, 1<<pin_bitnum[i]);
	}
	arm_dcache_flush_delete(dest, count * 128);
	//digitalWriteFast(12, LOW);

	// queue it for the next DMA transfer
	dma2next.TCD->SADDR = dest;
	dma2next.TCD->CITER_ELINKNO = count * 8;
	uint32_t remain = numbytes - (index + count);
	if (remain == 0) {
		dma2next.TCD->CSR = DMA_TCD_CSR_DREQ;
	} else if (remain <= BYTES_PER_DMA) {
		dma2next.TCD->CSR = DMA_TCD_CSR_ESG;
	} else {
		dma2next.TCD->CSR = DMA_TCD_CSR_ESG | DMA_TCD_CSR_INTMAJOR;
	}
}	// isr()


int ObjectFLED::busy(void)
{
	if (micros() - update_begin_micros < numbytes * TH_TL / OC_FACTOR / 1000 * 8 + LATCH_DELAY) {
		return 1;
	}
	return 0;
}


void ObjectFLED::setBrightness(uint8_t brightLevel) {
	brightness = brightLevel;
	rLevel = brightness * (colorBalance >> 16);
	gLevel = brightness * ((colorBalance >> 8) & 0xFF);
	bLevel = brightness * (colorBalance & 0xFF);
	}


void ObjectFLED::setBalance(uint32_t balMask) {
	colorBalance = balMask & 0xFFFFFF;
	rLevel = brightness * (colorBalance >> 16);
	gLevel = brightness * ((colorBalance >> 8) & 0xFF);
	bLevel = brightness * (colorBalance & 0xFF);
}


//Fades CRGB array towards the background color by amount.
void fadeToColorBy(void* leds, uint16_t count, uint32_t color, uint8_t fadeAmt) {
	for (uint32_t x = 0; x < count * 3; x += 3) {
		//fade red
		*((uint8_t*)leds + x) = ((  *((uint8_t*)leds + x)  * (1 + (255 - fadeAmt))) >> 8) + \
			((  ((color >> 16) & 0xFF)   * (1 + fadeAmt)) >> 8);
		//fade green
		*((uint8_t*)leds + x + 1) = ((  *((uint8_t*)leds + x + 1)   * (1 + (255 - fadeAmt))) >> 8) + \
			((  ((color >> 8) & 0xFF)   * (1 + fadeAmt)) >> 8);
		//fade blue
		*((uint8_t*)leds + x + 2) = ((  *((uint8_t*)leds + x + 2)   * (1 + (255 - fadeAmt))) >> 8) + \
			((  (color & 0xFF)   * (1 + fadeAmt)) >> 8);
	}
} //fadeToColorBy()


// Safely draws box even if partially offscreen on 2D CRGB array
void drawSquare(void* leds, uint16_t planeY, uint16_t planeX, int yCorner, int xCorner, uint32_t size, uint32_t color) {
	if (size != 0) { size--; }
	else { return; }
	for (int x = xCorner; x <= xCorner + (int)size; x++) {
		// if validX { if validY+S {draw Y+S,X}; if validY {draw Y, X} }
		if ((x >= 0) && (x < planeX)) {      //valid X
			if ((yCorner >= 0) && (yCorner < planeY)) {
				*((uint8_t*)leds + (yCorner * planeX + x) * 3) = ((color >> 16) & 0xFF);
				*((uint8_t*)leds + (yCorner * planeX + x) * 3 + 1) = ((color >> 8) & 0xFF);
				*((uint8_t*)leds + (yCorner * planeX + x) * 3 + 2) = (color & 0xFF);
			}
			if ((yCorner + size >= 0) && (yCorner + size < planeY)) {
				*((uint8_t*)leds + ((yCorner + size) * planeX + x) * 3) = ((color >> 16) & 0xFF);
				*((uint8_t*)leds + ((yCorner + size) * planeX + x) * 3 + 1) = ((color >> 8) & 0xFF);
				*((uint8_t*)leds + ((yCorner + size) * planeX + x) * 3 + 2) = (color & 0xFF);
			}
		}	//if valid x
	}	//for x
	for (int y = yCorner; y <= yCorner + (int)size; y++) {
		if ((y >= 0) && (y < planeY)) {      //valid y
			if ((xCorner >= 0) && (xCorner < planeX)) {
				*((uint8_t*)leds + (xCorner + y * planeX) * 3) = ((color >> 16) & 0xFF);
				*((uint8_t*)leds + (xCorner + y * planeX) * 3 + 1) = ((color >> 8) & 0xFF);
				*((uint8_t*)leds + (xCorner + y * planeX) * 3 + 2) = (color & 0xFF);
			}
			if ((xCorner + size >= 0) && (xCorner + size < planeX)) {
				*((uint8_t*)leds + (xCorner + size + y * planeX) * 3) = ((color >> 16) & 0xFF);
				*((uint8_t*)leds + (xCorner + size + y * planeX) * 3 + 1) = ((color >> 8) & 0xFF);
				*((uint8_t*)leds + (xCorner + size + y * planeX) * 3 + 2) = (color & 0xFF);
			}
		}	//if valid y
	}	//for y
} // drawSquare()

} // namespace fl


#endif // __IMXRT1062__
