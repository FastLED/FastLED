#ifndef __INC_CHIPSETS_H
#define __INC_CHIPSETS_H

#include "FastLED.h"
#include "pixeltypes.h"

///@file chipsets.h
/// contains the bulk of the definitions for the various LED chipsets supported.

FASTLED_NAMESPACE_BEGIN
///@defgroup chipsets
/// Implementations of CLEDController classes for various led chipsets.
///
///@{

#if defined(ARDUINO) //&& defined(SoftwareSerial_h)


#if defined(SoftwareSerial_h)
#include "SoftwareSerial.h"

#define HAS_PIXIE

/// Adafruit Pixie controller class
/// @tparam DATAPIN the pin to write data out on
/// @tparam RGB_ORDER the RGB ordering for the led data
template<uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class PixieController : public CPixelLEDController<RGB_ORDER> {
	SoftwareSerial Serial;
	CMinWait<2000> mWait;
public:
	PixieController() : Serial(-1, DATA_PIN) {}

protected:
	virtual void init() {
		Serial.begin(115200);
		mWait.mark();
	}

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWait.wait();
		while(pixels.has(1)) {
			uint8_t r = pixels.loadAndScale0();
			Serial.write(r);
			uint8_t g = pixels.loadAndScale1();
			Serial.write(g);
			uint8_t b = pixels.loadAndScale2();
			Serial.write(b);
			pixels.advanceData();
			pixels.stepDithering();
		}
		mWait.mark();
	}

};

// template<SoftwareSerial & STREAM, EOrder RGB_ORDER = RGB>
// class PixieController : public PixieBaseController<STREAM, RGB_ORDER> {
// public:
// 	virtual void init() {
// 		STREAM.begin(115200);
// 	}
// };
#endif
#endif

///@name Clocked chipsets - nominally SPI based these chipsets have a data and a clock line.
///@{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// LPD8806 controller class.
/// @tparam DATA_PIN the data pin for these leds
/// @tparam CLOCK_PIN the clock pin for these leds
/// @tparam RGB_ORDER the RGB ordering for these leds
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(12)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint8_t SPI_SPEED = DATA_RATE_MHZ(12) >
class LPD8806Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;

	class LPD8806_ADJUST {
	public:
		// LPD8806 spec wants the high bit of every rgb data byte sent out to be set.
		__attribute__((always_inline)) inline static uint8_t adjust(register uint8_t data) { return ((data>>1) | 0x80) + ((data && (data<254)) & 0x01); }
		__attribute__((always_inline)) inline static void postBlock(int len) {
			SPI::writeBytesValueRaw(0, ((len*3+63)>>6));
		}

	};

	SPI mSPI;
public:

	LPD8806Controller()  {}
	virtual void init() {
		mSPI.init();
	}

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.template writePixels<0, LPD8806_ADJUST, RGB_ORDER>(pixels);
	}
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// WS2801 controller class.
/// @tparam DATA_PIN the data pin for these leds
/// @tparam CLOCK_PIN the clock pin for these leds
/// @tparam RGB_ORDER the RGB ordering for these leds
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(1)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(1)>
class WS2801Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	CMinWait<1000>  mWaitDelay;
public:
	WS2801Controller() {}

	virtual void init() {
		mSPI.init();
	  mWaitDelay.mark();
	}

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWaitDelay.wait();
		mSPI.template writePixels<0, DATA_NOP, RGB_ORDER>(pixels);
		mWaitDelay.mark();
	}
};

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(25)>
class WS2803Controller : public WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// APA102 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// APA102 controller class.
/// @tparam DATA_PIN the data pin for these leds
/// @tparam CLOCK_PIN the clock pin for these leds
/// @tparam RGB_ORDER the RGB ordering for these leds
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(12)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(12)>
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() { mSPI.writeWord(0); mSPI.writeWord(0); }
	void endBoundary(int nLeds) { int nDWords = (nLeds/32); do { mSPI.writeByte(0xFF); mSPI.writeByte(0x00); mSPI.writeByte(0x00); mSPI.writeByte(0x00); } while(nDWords--); }

	inline void writeLed(uint8_t b0, uint8_t b1, uint8_t b2) __attribute__((always_inline)) {
		mSPI.writeByte(0xFF); mSPI.writeByte(b0); mSPI.writeByte(b1); mSPI.writeByte(b2);
	}

public:
	APA102Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		startBoundary();
		while(pixels.has(1)) {
#ifdef FASTLED_SPI_BYTE_ONLY
			mSPI.writeByte(0xFF);
			mSPI.writeByte(pixels.loadAndScale0());
			mSPI.writeByte(pixels.loadAndScale1());
			mSPI.writeByte(pixels.loadAndScale2());
#else
			uint16_t b = 0xFF00 | (uint16_t)pixels.loadAndScale0();
			mSPI.writeWord(b);
			uint16_t w = pixels.loadAndScale1() << 8;
			w |= pixels.loadAndScale2();
			mSPI.writeWord(w);
#endif
			pixels.stepDithering();
			pixels.advanceData();
		}
		endBoundary(pixels.size());
		mSPI.waitFully();
		mSPI.release();
	}

};

/// SK9822 controller class.
/// @tparam DATA_PIN the data pin for these leds
/// @tparam CLOCK_PIN the clock pin for these leds
/// @tparam RGB_ORDER the RGB ordering for these leds
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(24)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(24)>
class SK9822Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() { mSPI.writeWord(0); mSPI.writeWord(0); }
	void endBoundary(int nLeds) { int nLongWords = (nLeds/32); do { mSPI.writeByte(0x00); mSPI.writeByte(0x00); mSPI.writeByte(0x00); mSPI.writeByte(0x00); } while(nLongWords--); }

	inline void writeLed(uint8_t b0, uint8_t b1, uint8_t b2) __attribute__((always_inline)) {
		mSPI.writeByte(0xFF); mSPI.writeByte(b0); mSPI.writeByte(b1); mSPI.writeByte(b2);
	}

public:
	SK9822Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		startBoundary();
		while(pixels.has(1)) {
#ifdef FASTLED_SPI_BYTE_ONLY
			mSPI.writeByte(0xFF);
			mSPI.writeByte(pixels.loadAndScale0());
			mSPI.writeByte(pixels.loadAndScale1());
			mSPI.writeByte(pixels.loadAndScale2());
#else
			uint16_t b = 0xFF00 | (uint16_t)pixels.loadAndScale0();
			mSPI.writeWord(b);
			uint16_t w = pixels.loadAndScale1() << 8;
			w |= pixels.loadAndScale2();
			mSPI.writeWord(w);
#endif
			pixels.stepDithering();
			pixels.advanceData();
		}

		endBoundary(pixels.size());

		mSPI.waitFully();
		mSPI.release();
	}

};



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// P9813 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// P9813 controller class.
/// @tparam DATA_PIN the data pin for these leds
/// @tparam CLOCK_PIN the clock pin for these leds
/// @tparam RGB_ORDER the RGB ordering for these leds
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(10)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(10)>
class P9813Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void writeBoundary() { mSPI.writeWord(0); mSPI.writeWord(0); }

	inline void writeLed(uint8_t r, uint8_t g, uint8_t b) __attribute__((always_inline)) {
		register uint8_t top = 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
		mSPI.writeByte(top); mSPI.writeByte(b); mSPI.writeByte(g); mSPI.writeByte(r);
	}

public:
	P9813Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		writeBoundary();
		while(pixels.has(1)) {
			writeLed(pixels.loadAndScale0(), pixels.loadAndScale1(), pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
		writeBoundary();
		mSPI.waitFully();

		mSPI.release();
	}

};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SM16716 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// SM16716 controller class.
/// @tparam DATA_PIN the data pin for these leds
/// @tparam CLOCK_PIN the clock pin for these leds
/// @tparam RGB_ORDER the RGB ordering for these leds
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(16)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint8_t SPI_SPEED = DATA_RATE_MHZ(16)>
class SM16716Controller : public CPixelLEDController<RGB_ORDER> {
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

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		// writeHeader();
		mSPI.template writePixels<FLAG_START_BIT, DATA_NOP, RGB_ORDER>( pixels );
		writeHeader();
	}

};
/// @}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations - see clockless.h for how the timing values are used
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef FASTLED_HAS_CLOCKLESS
/// @name clockless controllers
/// Provides timing definitions for the variety of clockless controllers supplied by the library.
/// @{

// We want to force all avr's to use the Trinket controller when running at 8Mhz, because even the 328's at 8Mhz
// need the more tightly defined timeframes.
#if (F_CPU == 8000000 || F_CPU == 16000000 || F_CPU == 24000000) //  || F_CPU == 48000000 || F_CPU == 96000000) // 125ns/clock
#define FMUL (F_CPU/8000000)
// LPD1886
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 3 * FMUL, 2 * FMUL, RGB_ORDER, 4> {};

// LPD1886
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz_8bit : public ClocklessController<DATA_PIN, 2 * FMUL, 3 * FMUL, 2 * FMUL, RGB_ORDER> {};

// WS2811@800khz 2 clocks, 5 clocks, 3 clocks
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2812Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 4 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 10 * FMUL, 6 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6822Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 8 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6812Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 3 * FMUL, 4 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 12 * FMUL, 4 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 4 * FMUL, 4 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 3 * FMUL, 4 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS2903Controller : public ClocklessController<DATA_PIN, 2 * FMUL, 6 * FMUL, 2 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, 6 * FMUL, 9 * FMUL, 6 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller400Khz : public ClocklessController<DATA_PIN, 6 * FMUL, 7 * FMUL, 6 * FMUL, RGB_ORDER, 4> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 4 * FMUL, 4 * FMUL, RGB_ORDER, 4> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class PL9823Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 8 * FMUL, 3 * FMUL, RGB_ORDER> {};

#else
// GW6205@400khz - 800ns, 800ns, 800ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller400Khz : public ClocklessController<DATA_PIN, NS(800), NS(800), NS(800), RGB_ORDER, 4> {};

// GW6205@400khz - 400ns, 400ns, 400ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller800Khz : public ClocklessController<DATA_PIN, NS(400), NS(400), NS(400), RGB_ORDER, 4> {};

// UCS1903 - 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, NS(500), NS(1500), NS(500), RGB_ORDER> {};

// UCS1903B - 400ns, 450ns, 450ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public ClocklessController<DATA_PIN, NS(400), NS(450), NS(450), RGB_ORDER> {};

// UCS1904 - 400ns, 400ns, 450ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public ClocklessController<DATA_PIN, NS(400), NS(400), NS(450), RGB_ORDER> {};

// UCS2903 - 250ns, 750ns, 250ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS2903Controller : public ClocklessController<DATA_PIN, NS(250), NS(750), NS(250), RGB_ORDER> {};

// TM1809 - 350ns, 350ns, 550ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(450), RGB_ORDER> {};

// WS2811 - 320ns, 320ns, 640ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, NS(320), NS(320), NS(640), RGB_ORDER> {};

// WS2812 - 250ns, 625ns, 375ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2812Controller800Khz : public ClocklessController<DATA_PIN, NS(250), NS(625), NS(375), RGB_ORDER> {};

// WS2811@400khz - 800ns, 800ns, 900ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, NS(800), NS(800), NS(900), RGB_ORDER> {};

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, NS(700), NS(1100), NS(700), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller800Khz : public ClocklessController<DATA_PIN, NS(340), NS(340), NS(550), RGB_ORDER, 0, true, 500> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller1600Khz : public ClocklessController<DATA_PIN, NS(100), NS(300), NS(200), RGB_ORDER, 0, true, 500> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz : public ClocklessController<DATA_PIN, NS(200), NS(400), NS(200), RGB_ORDER, 4> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz_8bit : public ClocklessController<DATA_PIN, NS(200), NS(400), NS(200), RGB_ORDER> {};


template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6822Controller : public ClocklessController<DATA_PIN, NS(375), NS(1000), NS(375), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6812Controller : public ClocklessController<DATA_PIN, NS(300), NS(300), NS(600), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class PL9823Controller : public ClocklessController<DATA_PIN, NS(350), NS(1010), NS(350), RGB_ORDER> {};
#endif
///@}

#endif
///@}
FASTLED_NAMESPACE_END

#endif
