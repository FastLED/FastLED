#ifndef __INC_CHIPSETS_H
#define __INC_CHIPSETS_H

#include "pixeltypes.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SPI_SPEED> class LPD8806_ADJUST {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
public:
	// LPD8806 spec wants the high bit of every rgb data byte sent out to be set.
	__attribute__((always_inline)) inline static uint8_t adjust(register uint8_t data) { return (data>>1) | 0x80; }
	__attribute__((always_inline)) inline static uint8_t adjust(register uint8_t data, register uint8_t scale) { return (scale8(data, scale)>>1) | 0x80; }
	__attribute__((always_inline)) inline static void postBlock(int len) { 
		SPI::writeBytesValueRaw(0, 1); // ((len+63)>>6));
	}

};

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = 2 >
class LPD8806Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
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
		if(SELECT_PIN != NO_PIN) {  
			mSPI.setSelect(new OutputPin(SELECT_PIN));
		} else {
			// mSPI.setSelect(NULL);
		}
		mSPI.init();
		mClearedLeds = 0;
	}

	virtual void clearLeds(int nLeds) { 
		checkClear(nLeds);
		mSPI.writeBytesValue(0x80, nLeds * 3);	
	}

	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		checkClear(nLeds);
		mSPI.select();
		while(nLeds--) { 
			mSPI.writeByte(data[RGB_BYTE0(RGB_ORDER)]);
			mSPI.writeByte(data[RGB_BYTE1(RGB_ORDER)]);
			mSPI.writeByte(data[RGB_BYTE2(RGB_ORDER)]);
		}
		// latch in the world
		mSPI.writeBytesValueRaw(0, ((nLeds+63)>>6));
		mSPI.waitFully();
		mSPI.release();
	}

	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) {
		mSPI.template writeBytes3<LPD8806_ADJUST<DATA_PIN, CLOCK_PIN, SPI_SPEED>, RGB_ORDER>((byte*)data, nLeds * 3, scale);
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale) {
		checkClear(nLeds);
		mSPI.template writeBytes3<1, LPD8806_ADJUST<DATA_PIN, CLOCK_PIN, SPI_SPEED>, RGB_ORDER>((byte*)data, nLeds * 4, scale);
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = 3>
class WS2801Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	CMinWait<24>  mWaitDelay;
public:
	WS2801Controller() {}

	virtual void init() { 
		if(SELECT_PIN != NO_PIN) {  
			mSPI.setSelect(new OutputPin(SELECT_PIN));
		} else { 
			mSPI.setSelect(NULL);
		}
		mSPI.init();
	    // 0 out as much as we can on the line
	    mSPI.writeBytesValue(0, 1000);
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
		while(nLeds--) { 
			mSPI.writeByte(data[RGB_BYTE0(RGB_ORDER)]);
			mSPI.writeByte(data[RGB_BYTE1(RGB_ORDER)]);
			mSPI.writeByte(data[RGB_BYTE2(RGB_ORDER)]);
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
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SPI_SPEED> class SM16716_ADJUST {
#if defined(__MK20DX128__) 
		typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
#else
	// Have to force software SPI for the AVR right now because it doesn't deal well
	// with flipping in and out of hardware SPI
//		typedef AVRSoftwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
		typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
#endif
public:
	static __attribute__((always_inline)) inline uint8_t adjust(register uint8_t data) { return data; } 
	static __attribute__((always_inline)) inline uint8_t adjust(register uint8_t data, register uint8_t scale) { return scale8(data, scale); } 
	static __attribute__((always_inline)) inline void postBlock(int len) {
		// SPI::writeBytesValueRaw(0, 3);
	}
};


template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = 0>
class SM16716Controller : public CLEDController {
#if defined(__MK20DX128__)  
		typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
#else
	// Have to force software SPI for the AVR right now because it doesn't deal well
	// with flipping in and out of hardware SPI
		typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
//		typedef AVRSoftwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
#endif
	SPI mSPI;
	OutputPin selectPin;

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
	SM16716Controller() : selectPin(SELECT_PIN) {}

	virtual void init() { 
		mSPI.setSelect(&selectPin);
		mSPI.init();
	}

	virtual void clearLeds(int nLeds) { 
		mSPI.select();
		writeHeader();
		while(nLeds--) { 
			mSPI.template writeBit<0>(1);
			mSPI.writeByte(0);
			mSPI.writeByte(0);
			mSPI.writeByte(0);
		}
		mSPI.waitFully();
		mSPI.release();
	}

	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mSPI.select();
		writeHeader();
		while(nLeds--) { 
			mSPI.template writeBit<0>(1);
			mSPI.writeByte(data[RGB_BYTE0(RGB_ORDER)]);
			mSPI.writeByte(data[RGB_BYTE1(RGB_ORDER)]);
			mSPI.writeByte(data[RGB_BYTE2(RGB_ORDER)]);
		}
		writeHeader();
		mSPI.release();
	}

	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) {
		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		// writeHeader();
		mSPI.template writeBytes3<FLAG_START_BIT, SM16716_ADJUST<DATA_PIN, CLOCK_PIN, SPI_SPEED>, RGB_ORDER>((byte*)data, nLeds * 3, scale);
		writeHeader();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale = 255) {
		mSPI.writeBytesValue(0, 6);
		mSPI.template writeBit<0>(0);
		mSPI.template writeBit<0>(0);

		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		mSPI.template writeBytes3<1 | FLAG_START_BIT, SM16716_ADJUST<DATA_PIN, CLOCK_PIN, SPI_SPEED>, RGB_ORDER>((byte*)data, nLeds * 4, scale);
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// UCS1903 - 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Mhz : public ClocklessController<DATA_PIN, NS(500), NS(1500), NS(500), RGB_ORDER> {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, NS(500), NS(1500), NS(500), RGB_ORDER> {};
#if NO_TIME(500, 1500, 500) 
#warning "No enough clock cycles available for the UCS103"
#endif

// TM1809 - 312.5ns, 312.5ns, 325ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Mhz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(550), RGB_ORDER> {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(550), RGB_ORDER> {};
#if NO_TIME(350, 350, 550) 
#warning "No enough clock cycles available for the UCS103"
#endif

// WS2811 - 350n, 350ns, 550ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Mhz : public ClocklessController<DATA_PIN, NS(320), NS(320), NS(550), RGB_ORDER> {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, NS(320), NS(320), NS(550), RGB_ORDER> {};
#if NO_TIME(320, 320, 550) 
#warning "No enough clock cycles available for the UCS103"
#endif

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Mhz : public ClocklessController<DATA_PIN, NS(750), NS(750), NS(750), RGB_ORDER> {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, NS(750), NS(750), NS(750), RGB_ORDER> {};
#if NO_TIME(750, 750, 750) 
#warning "No enough clock cycles available for the UCS103"
#endif

#endif
