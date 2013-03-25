#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include "controller.h"
#include "fastpin.h"
#include "fastspi.h"
#include "clockless.h"


// Class to ensure that a minimum amount of time has kicked since the last time run - and delay if not enough time has passed yet
// this should make sure that chipsets that have 
template<int WAIT> class CMinWait {
	long mLastMicros;
public:
	CMinWait() { mLastMicros = 0; }

	void wait() { 
		long diff = micros() - mLastMicros;
		if(diff < WAIT) { 
			delayMicroseconds(WAIT - diff);
		}
	}

	void mark() { mLastMicros = micros(); }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class LPD8806_ADJUST {
public:
	// LPD8806 spec wants the high bit of every rgb data byte sent out to be set.
	__attribute__((always_inline)) inline static uint8_t adjust(register uint8_t data) { return (data>>1) | 0x80; }
};

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, uint8_t SPI_SPEED = 2 >
class LPD8806Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	OutputPin selectPin;

	void clearLine(int nLeds) { 
		int n = ((nLeds  + 63) >> 6);
		mSPI.writeBytesValue(0, n);
	}
public:
	LPD8806Controller() : selectPin(SELECT_PIN) {}
	virtual void init() { 
		mSPI.setSelect(&selectPin);
		mSPI.init();

		// push out 1000 leds worth of 0's to clear out the line
		mSPI.writeBytesValue(0x80, 1000);
		clearLine(1000);
	}

	virtual void showRGB(uint8_t *data, int nLeds) {
		mSPI.template writeBytes3<LPD8806_ADJUST>(data, nLeds * 3);
		clearLine(nLeds);
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(uint8_t *data, int nLeds) {
		mSPI.template writeBytes3<1, LPD8806_ADJUST>(data, nLeds * 4);
		clearLine(nLeds);
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, uint8_t SPI_SPEED = 3>
class WS2801Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	OutputPin selectPin;
	CMinWait<24>  mWaitDelay;
public:
	WS2801Controller() : selectPin(SELECT_PIN) {}

	virtual void init() { 
		mSPI.setSelect(&selectPin);
		mSPI.init();
	    // 0 out as much as we can on the line
	    mSPI.writeBytesValue(0, 1000);
	    mWaitDelay.mark();
	}

	virtual void showRGB(uint8_t *data, int nLeds) {
		// mWaitDelay.wait();
		mSPI.writeBytes3(data, nLeds * 3);
		// mWaitDelay.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(uint8_t *data, int nLeds) {
		mWaitDelay.wait();
		mSPI.template writeBytes3<1>(data, nLeds * 4);
		mWaitDelay.mark();
	}
#endif
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SM16716 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, uint8_t SPI_SPEED = 0>
class SM16716Controller : public CLEDController {
#if defined(__MK20DX128__)   // for Teensy 3.0
	// Have to force software SPI for the teensy 3.0 right now because it doesn't deal well
	// with flipping in and out of hardware SPI
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
#else
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
#endif
	SPI mSPI;
	OutputPin selectPin;
public:
	SM16716Controller() : selectPin(SELECT_PIN) {}

	virtual void init() { 
		mSPI.setSelect(&selectPin);
		mSPI.init();
	}

	virtual void showRGB(uint8_t *data, int nLeds) {
		// Write out 50 zeros to the spi line (6 blocks of 8 followed by two single bit writes)
		mSPI.writeBytesValue(0, 6);
		mSPI.template writeBit<0>(0);
		mSPI.template writeBit<0>(0);

		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		mSPI.template writeBytes3<FLAG_START_BIT>(data, nLeds * 3);
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(uint8_t *data, int nLeds) {
		mSPI.writeBytesValue(0, 6);
		mSPI.template writeBit<0>(0);
		mSPI.template writeBit<0>(0);

		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		mSPI.template writeBytes3<1 | FLAG_START_BIT>(data, nLeds * 4);
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN>
class UCS1903Controller400Mhz : public ClocklessController<DATA_PIN, NS(500), NS(1500), NS(500)> {};
#if NO_TIME(500, 1500, 500) 
#warning "No enough clock cycles available for the UCS103"
#endif

// 312.5ns, 312.5ns, 325ns
template <uint8_t DATA_PIN>
class TM1809Controller800Mhz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(550)> {};
#if NO_TIME(350, 350, 550) 
#warning "No enough clock cycles available for the UCS103"
#endif

// 350n, 350ns, 550ns
template <uint8_t DATA_PIN>
class WS2811Controller800Mhz : public ClocklessController<DATA_PIN, NS(320), NS(320), NS(550)> {};
#if NO_TIME(320, 320, 550) 
#warning "No enough clock cycles available for the UCS103"
#endif

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN>
class TM1803Controller400Mhz : public ClocklessController<DATA_PIN, NS(750), NS(750), NS(750)> {};
#if NO_TIME(750, 750, 750) 
#warning "No enough clock cycles available for the UCS103"
#endif

#endif