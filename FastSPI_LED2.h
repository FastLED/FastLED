#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include "controller.h"
#include "fastpin.h"
#include "fastspi.h"
#include "clockless.h"



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

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, uint8_t SPI_SPEED = 0 >
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

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SELECT_PIN, uint8_t SPI_SPEED = 1>
class WS2801Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	OutputPin selectPin;
public:
	WS2801Controller() : selectPin(SELECT_PIN) {}
	
	virtual void init() { 
		mSPI.setSelect(&selectPin);
		mSPI.init();
	    // 0 out as much as we can on the line
	    mSPI.writeBytesValue(0, 1000);
	}

	virtual void showRGB(uint8_t *data, int nLeds) {
		mSPI.writeBytes3(data, nLeds * 3);
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(uint8_t *data, int nLeds) {
		mSPI.template writeBytes3<1>(data, nLeds * 4);
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
#pragma message "No enough clock cycles available for the UCS103"
#endif

// 312.5ns, 312.5ns, 325ns
template <uint8_t DATA_PIN>
class TM1809Controller800Mhz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(550)> {};
#if NO_TIME(350, 350, 550) 
#pragma message "No enough clock cycles available for the UCS103"
#endif

// 350n, 350ns, 550ns
template <uint8_t DATA_PIN>
class WS2811Controller800Mhz : public ClocklessController<DATA_PIN, NS(320), NS(320), NS(550)> {};
#if NO_TIME(320, 320, 550) 
#pragma message "No enough clock cycles available for the UCS103"
#endif

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN>
class TM1803Controller400Mhz : public ClocklessController<DATA_PIN, NS(750), NS(750), NS(750)> {};
#if NO_TIME(750, 750, 750) 
#pragma message "No enough clock cycles available for the UCS103"
#endif

#endif