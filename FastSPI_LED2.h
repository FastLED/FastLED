#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include "controller.h"
#include "fastpin.h"
#include "fastspi.h"
#include "clockless.h"



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/latch pin values (N.B. should take an SPI definition)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class LPD8806_ADJUST {
public:
	static uint8_t adjust(register uint8_t data) { return (data>>1) | 0x80; }
};

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t LATCH_PIN>
class LPD8806Controller : public CLEDController {
	typedef ArduinoSPIOutput<DATA_PIN, CLOCK_PIN, LATCH_PIN, 0> SPI;

	void clearLine(int nLeds) { 
		int n = ((nLeds  + 63) >> 6);
		SPI::writeBytesValue(0, n);
	}
public:
	virtual void init() { 
		SPI::init();
		SPI::writeBytesValue(0x80, 1000);
		clearLine(1000);
	}

	virtual void showRGB(uint8_t *data, int nLeds) {
		SPI::template writeBytes3<LPD8806_ADJUST>(data, nLeds * 3);
		clearLine(nLeds);
	}

	virtual void showARGB(uint8_t *data, int nLeds) {
		SPI::template writeBytes3<1, LPD8806_ADJUST>(data, nLeds * 4);
		clearLine(nLeds);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t LATCH_PIN>
class WS2801Controller : public CLEDController {
	typedef ArduinoSPIOutput<DATA_PIN, CLOCK_PIN, LATCH_PIN, 0> SPI;

public:
	virtual void init() { 
		SPI::init();
	}

	virtual void showRGB(uint8_t *data, int nLeds) {
		SPI::template writeBytes3(data, nLeds * 3);
	}

	virtual void showARGB(uint8_t *data, int nLeds) {
		SPI::template writeBytes3<1>(data, nLeds * 4);
	}
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN>
class UCS1903Controller400Mhz : public ClocklessController<DATA_PIN, 8, 24, 8> {};

// 312.5ns, 312.5ns, 325ns
template <uint8_t DATA_PIN>
class TM1809Controller800Mhz : public ClocklessController<DATA_PIN, 5, 5, 10> {};

// 312.5ns, 312.5ns, 325ns
template <uint8_t DATA_PIN>
class WS2811Controller800Mhz : public ClocklessController<DATA_PIN, 5, 5, 10> {};

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN>
class TM1803Controller400Mhz : public ClocklessController<DATA_PIN, 12, 12, 12> {};

#endif