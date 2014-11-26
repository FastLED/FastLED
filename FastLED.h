#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

// #define NO_CORRECTION 1
// #define NO_DITHERING 1

#define xstr(s) str(s)
#define str(s) #s

#define  FASTLED_VERSION 3001000
#ifndef FASTLED_INTERNAL
#warning FastLED version 3001000  (Not really a warning, just telling you here.)
#endif

#ifndef __PROG_TYPES_COMPAT__
#define __PROG_TYPES_COMPAT__
#endif

#ifdef SmartMatrix_h
#include<SmartMatrix.h>
#endif

#ifdef DmxSimple_h
#include<DmxSimple.h>
#endif

#ifdef DmxSerial_h
#include<DMXSerial.h>
#endif

#include <stdint.h>

#include "fastled_config.h"
#include "led_sysdefs.h"

#include "bitswap.h"
#include "controller.h"
#include "fastpin.h"
#include "fastspi_types.h"
#include "./dmx.h"

#include "platforms.h"

#include "lib8tion.h"
#include "hsv2rgb.h"
#include "colorutils.h"
#include "colorpalettes.h"

#include "noise.h"
#include "power_mgt.h"

#include "fastspi.h"
#include "chipsets.h"

enum ESPIChipsets {
	LPD8806,
	WS2801,
	WS2803,
	SM16716,
	P9813,
	APA102
};

enum ESM { SMART_MATRIX };
enum OWS2811 { OCTOWS2811 };

template<uint8_t DATA_PIN> class NEOPIXEL : public WS2811Controller800Khz<DATA_PIN, GRB> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1829 : public TM1829Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1809 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1804 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1803 : public TM1803Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class UCS1903 : public UCS1903Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class UCS1903B : public UCS1903BController800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2812 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2812B : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2811 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class APA104 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2811_400 : public WS2811Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class GW6205 : public GW6205Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class GW6205_400 : public GW6205Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class LPD1886 : public LPD1886Controller1250Khz<DATA_PIN, RGB_ORDER> {};
#ifdef DmxSimple_h
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class DMXSIMPLE : public DMXSimpleController<DATA_PIN, RGB_ORDER> {};
#endif
#ifdef DmxSerial_h
template<EOrder RGB_ORDER> class DMXSERIAL : public DMXSerialController<RGB_ORDER> {};
#endif

// template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = DATA_RATE_MHZ(20)> class LPD8806 : public LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};
// template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = DATA_RATE_MHZ(1)> class WS2801 : public WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};
// template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = DATA_RATE_MHZ(15)> class P9813 : public P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};
// template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = DATA_RATE_MHZ(16)> class SM16716 : public SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};

enum EBlockChipsets {
#ifdef PORTA_FIRST_PIN
	WS2811_PORTA,
#endif
#ifdef PORTB_FIRST_PIN
	WS2811_PORTB,
#endif
#ifdef PORTC_FIRST_PIN
	WS2811_PORTC,
#endif
#ifdef PORTD_FIRST_PIN
	WS2811_PORTD,
#endif
#ifdef HAS_PORTDC
	WS2811_PORTDC,
#endif
};

#if defined(LIB8_ATTINY)
#define NUM_CONTROLLERS 2
#else
#define NUM_CONTROLLERS 8
#endif

class CFastLED {
	// int m_nControllers;
	uint8_t  m_Scale;
	uint16_t m_nFPS;
	uint32_t m_nMinMicros;
public:
	CFastLED();

	static CLEDController &addLeds(CLEDController *pLed, struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN > static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2803: { static WS2803Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case P9813: { static P9813Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case APA102: { static APA102Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER > static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2803: { static WS2803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case P9813: { static P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case APA102: { static APA102Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE > CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2803: { static WS2803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case P9813: { static P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case APA102: { static APA102Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

#ifdef SPI_DATA
	template<ESPIChipsets CHIPSET> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER, SPI_DATA_RATE>(data, nLedsOrOffset, nLedsIfOffset);
	}

#endif

	template<template<uint8_t DATA_PIN> class CHIPSET, uint8_t DATA_PIN>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<EOrder RGB_ORDER> class CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<EOrder RGB_ORDER> class CHIPSET, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

#ifdef USE_OCTOWS2811
template<OWS2811 CHIPSET, EOrder RGB_ORDER>
static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
{
	switch(CHIPSET) {
		case OCTOWS2811: { static COctoWS2811Controller<RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
	}
}

template<OWS2811 CHIPSET>
static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
{
	return addLeds<CHIPSET,GRB>(data,nLedsOrOffset,nLedsIfOffset);
}

#endif

#ifdef SmartMatrix_h
	template<ESM CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case SMART_MATRIX: { static CSmartMatrixController controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
#endif

#ifdef FASTSPI_USE_DMX_SIMPLE
	template<EClocklessChipsets CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER=RGB>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	 {
		switch(CHIPSET) {
			case DMX: { static DMXController<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
#endif

#ifdef HAS_BLOCKLESS
template<EBlockChipsets CHIPSET, int NUM_LANES, EOrder RGB_ORDER>
static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
	switch(CHIPSET) {
#ifdef PORTA_FIRST_PIN
		case WS2811_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(250), NS(510), NS(490), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
#ifdef PORTB_FIRST_PIN
		case WS2811_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(250), NS(510), NS(490), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
#ifdef PORTC_FIRST_PIN
		case WS2811_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(250), NS(510), NS(490), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
#ifdef PORTD_FIRST_PIN
		case WS2811_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(250), NS(510), NS(490), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
#ifdef HAS_PORTDC
		case WS2811_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<16,NS(250), NS(510), NS(490), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
	}

}

template<EBlockChipsets CHIPSET, int NUM_LANES>
static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
	return addLeds<CHIPSET,NUM_LANES,GRB>(data,nLedsOrOffset,nLedsIfOffset);
}
#endif

	void setBrightness(uint8_t scale) { m_Scale = scale; }
	uint8_t getBrightness() { return m_Scale; }

	/// Update all our controllers with the current led colors, using the passed in brightness
	void show(uint8_t scale);

	/// Update all our controllers with the current led colors
	void show() { show(m_Scale); }

	void clear(boolean writeData = false);

	void clearData();

	void showColor(const struct CRGB & color, uint8_t scale);

	void showColor(const struct CRGB & color) { showColor(color, m_Scale); }

	void delay(unsigned long ms);

	void setTemperature(const struct CRGB & temp);
	void setCorrection(const struct CRGB & correction);
	void setDither(uint8_t ditherMode = BINARY_DITHER);

	// Set the maximum refresh rate.  This is global for all leds.  Attempts to
	// call show faster than this rate will simply wait.  Defaults to 400Hz.  Set
	// to 0 to have no maximum rate.
	void setMaxRefreshRate(uint16_t refresh);

	// for debugging, will keep track of time between calls to countFPS, and every
	// nFrames calls, it will update an internal counter for the current FPS.
	void countFPS(int nFrames=25);
	// Get the number of frames/second being written out
	uint16_t getFPS() { return m_nFPS; }

    // returns the number of controllers (strips) that have been added with addLeds
	int count();

    // returns the Nth controller
	CLEDController & operator[](int x);

    // Convenience functions for single-strip setups:

    // returns the number of LEDs in the first strip
	int size() { return (*this)[0].size(); }

    // returns pointer to the CRGB buffer for the first strip
	CRGB *leds() { return (*this)[0].leds(); }
};

#ifdef FASTLED_AVR
extern "C" {
	unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long);
void delayMicroseconds(unsigned int us);
};
#endif

#define FastSPI_LED FastLED
#define FastSPI_LED2 FastLED
#ifndef LEDS
#define LEDS FastLED
#endif

extern CFastLED FastLED;

// Warnings for undefined things
#ifndef HAS_HARDWARE_PIN_SUPPORT
#warning "No pin/port mappings found, pin access will be slightly slower.  See fastpin.h for info."
#define NO_HARDWARE_PIN_SUPPORT
#endif


#endif
