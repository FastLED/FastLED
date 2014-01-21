#ifndef __INC_CHIPSETS_H
#define __INC_CHIPSETS_H

#include "pixeltypes.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = DATA_RATE_MHZ(24) >
class LPD8806Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;

	class LPD8806_ADJUST {
	public:
		// LPD8806 spec wants the high bit of every rgb data byte sent out to be set.
		__attribute__((always_inline)) inline static uint8_t adjust(register uint8_t data) { return (data>>1) | 0x80; }
		__attribute__((always_inline)) inline static uint8_t adjust(register uint8_t data, register uint8_t scale) { return (scale8(data, scale)>>1) | 0x80; }
		__attribute__((always_inline)) inline static void postBlock(int len) { 
			SPI::writeBytesValueRaw(0, ((len+63)>>6));
		}

	};

	SPI mSPI;
	int mClearedLeds;

	void checkClear(int nLeds) { 
		if(nLeds > mClearedLeds) { 
			clearLine(nLeds);
			mClearedLeds = nLeds;
		}
	}
	
	void clearLine(int nLeds) { 
		int n = ((nLeds  + 63) >> 6);
		mSPI.writeBytesValue(0, n);
	}
public:
	LPD8806Controller()  {}
	virtual void init() {
		mSPI.init();
		mClearedLeds = 0;
	}

	virtual void clearLeds(int nLeds) { 
		mSPI.select();
		mSPI.writeBytesValueRaw(0x80, nLeds * 3);	
		mSPI.writeBytesValueRaw(0, ((nLeds*3+63)>>6));
		mSPI.release();
	}

	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mSPI.select();
		uint8_t a = 0x80 | (scale8(data[RGB_BYTE0(RGB_ORDER)], scale) >> 1);
		uint8_t b = 0x80 | (scale8(data[RGB_BYTE1(RGB_ORDER)], scale) >> 1);
		uint8_t c = 0x80 | (scale8(data[RGB_BYTE2(RGB_ORDER)], scale) >> 1);
		int iLeds = 0;

		while(iLeds++ < nLeds) { 
			mSPI.writeByte(a);
			mSPI.writeByte(b);
			mSPI.writeByte(c);
		}

		// latch in the world
		mSPI.writeBytesValueRaw(0, ((nLeds*3+63)>>6));
		mSPI.release();
	}

	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) {
		mSPI.template writeBytes3<LPD8806_ADJUST, RGB_ORDER>((byte*)data, nLeds * 3, scale);
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale) {
		checkClear(nLeds);
		mSPI.template writeBytes3<1, LPD8806_ADJUST, RGB_ORDER>((byte*)data, nLeds * 4, scale);
	}
#endif
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(1)>
class WS2801Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	CMinWait<1000>  mWaitDelay;
public:
	WS2801Controller() {}

	virtual void init() { 
		mSPI.init();
	    mWaitDelay.mark();
	}

	virtual void clearLeds(int nLeds) { 
		mWaitDelay.wait();
		mSPI.writeBytesValue(0, nLeds*3);
		mWaitDelay.mark();
	}
	
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mWaitDelay.wait();
		mSPI.select();
		uint8_t a = scale8(data[RGB_BYTE0(RGB_ORDER)], scale);
		uint8_t b = scale8(data[RGB_BYTE1(RGB_ORDER)], scale);
		uint8_t c = scale8(data[RGB_BYTE2(RGB_ORDER)], scale);

		while(nLeds--) { 
			mSPI.writeByte(a);
			mSPI.writeByte(b);
			mSPI.writeByte(c);
		}
		mSPI.waitFully();
		mSPI.release();
		mWaitDelay.mark();
	}

	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale) {
		mWaitDelay.wait();
		mSPI.template writeBytes3<0, RGB_ORDER>((byte*)data, nLeds * 3, scale);
		mWaitDelay.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale) {
		mWaitDelay.wait();
		mSPI.template writeBytes3<1, RGB_ORDER>((byte*)data, nLeds * 4, scale);
		mWaitDelay.mark();
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SM16716 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(16)>
class SM16716Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void writeHeader() { 
		// Write out 50 zeros to the spi line (6 blocks of 8 followed by two single bit writes)
		mSPI.select();
		mSPI.writeBytesValueRaw(0, 6);
		mSPI.waitFully();
		mSPI.template writeBit<0>(0);
		mSPI.template writeBit<0>(0);
		mSPI.release();
	}

public:
	SM16716Controller() {}

	virtual void init() { 
		mSPI.init();
	}

	virtual void clearLeds(int nLeds) { 
		mSPI.select();
		while(nLeds--) { 
			mSPI.template writeBit<0>(1);
			mSPI.writeByte(0);
			mSPI.writeByte(0);
			mSPI.writeByte(0);
		}
		mSPI.waitFully();
		mSPI.release();
		writeHeader();
	}

	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mSPI.select();
		uint8_t a = scale8(data[RGB_BYTE0(RGB_ORDER)], scale);
		uint8_t b = scale8(data[RGB_BYTE1(RGB_ORDER)], scale);
		uint8_t c = scale8(data[RGB_BYTE2(RGB_ORDER)], scale);

		while(nLeds--) { 
			mSPI.template writeBit<0>(1);
			mSPI.writeByte(a);
			mSPI.writeByte(b);
			mSPI.writeByte(c);
		}
		writeHeader();
		mSPI.release();
	}

	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) {
		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		// writeHeader();
		mSPI.template writeBytes3<FLAG_START_BIT, RGB_ORDER>((byte*)data, nLeds * 3, scale);
		writeHeader();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale = 255) {
		mSPI.writeBytesValue(0, 6);
		mSPI.template writeBit<0>(0);
		mSPI.template writeBit<0>(0);

		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		mSPI.template writeBytes3<1 | FLAG_START_BIT, RGB_ORDER>((byte*)data, nLeds * 4, scale);
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations - see clockless.h for how the timing values are used
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(LIB8_ATTINY) && (F_CPU == 8000000)
// WS2811@8Mhz 2 clocks, 5 clocks, 3 clocks
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController_Trinket<DATA_PIN, 2, 5, 3, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController_Trinket<DATA_PIN, 4, 10, 6, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController_Trinket<DATA_PIN, 4, 12, 4, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, 2, 5, 3, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, 6, 6, 6, RGB_ORDER> {};

#elif defined(LIB8_ATTINY) && (F_CPU == 16000000)

// WS2811@16Mhz 4 clocks, 10 clocks, 6 clocks
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController_Trinket<DATA_PIN, 4, 10, 6, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController_Trinket<DATA_PIN, 8, 20, 12, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController_Trinket<DATA_PIN, 8, 24, 8, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, 4, 10, 6, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, 12, 12, 12, RGB_ORDER> {};

#else
// UCS1903 - 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, NS(500), NS(1500), NS(500), RGB_ORDER> {};
#if NO_TIME(500, 1500, 500) 
#warning "Not enough clock cycles available for the UCS103"
#endif

// TM1809 - 350ns, 350ns, 550ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(550), RGB_ORDER> {};
#if NO_TIME(350, 350, 550) 
#warning "Not enough clock cycles available for the TM1809"
#endif

// WS2811 - 400ns, 400ns, 450ns 
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, NS(400), NS(400), NS(450), RGB_ORDER> {};
#if NO_TIME(400, 400, 450) 
#warning "Not enough clock cycles available for the WS2811 (800khz)"
#endif

// WS2811@400khz - 800ns, 800ns, 900ns 
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, NS(800), NS(800), NS(900), RGB_ORDER> {};
#if NO_TIME(800, 800, 900) 
#warning "Not enough clock cycles available for the WS2811 (400Khz)"
#endif

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, NS(750), NS(750), NS(750), RGB_ORDER> {};
#if NO_TIME(750, 750, 750) 
#warning "Not enough clock cycles available for the UCS103"
#endif
#endif

#endif
