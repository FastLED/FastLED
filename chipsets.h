#ifndef __INC_CHIPSETS_H
#define __INC_CHIPSETS_H

#include "pixeltypes.h"

///@file chipsets.h
/// contains the bulk of the definitions for the various LED chipsets supported.

FASTLED_NAMESPACE_BEGIN
///@defgroup chipsets
/// Implementations of CLEDController classes for various led chipsets.
///
///@{

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
class LPD8806Controller : public CLEDController {
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
	int mClearedLeds;

	void checkClear(int nLeds) {
		if(nLeds > mClearedLeds) {
			clearLine(nLeds);
			mClearedLeds = nLeds;
		}
	}

	void clearLine(int nLeds) {
		int n = ((nLeds*3  + 63) >> 6);
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
		mSPI.waitFully();
		mSPI.release();
	}

protected:

	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		mSPI.template writePixels<0, LPD8806_ADJUST, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
	}

	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		// TODO rgb-ize scale
		mSPI.template writePixels<0, LPD8806_ADJUST, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale) {
		checkClear(nLeds);
		mSPI.template writePixels<0, LPD8806_ADJUST, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
	}
#endif
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

protected:

	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		mWaitDelay.wait();
		mSPI.template writePixels<0, DATA_NOP, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
		mWaitDelay.mark();
	}

	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		mWaitDelay.wait();
		mSPI.template writePixels<0, DATA_NOP, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
		mWaitDelay.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		mWaitDelay.wait();
		mSPI.template writePixels<0, DATA_NOP, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
		mWaitDelay.mark();
	}
#endif
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
/// @tparam SPI_SPEED the clock divider used for these leds.  Set using the DATA_RATE_MHZ/DATA_RATE_KHZ macros.  Defaults to DATA_RATE_MHZ(24)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = BGR, uint8_t SPI_SPEED = DATA_RATE_MHZ(24)>
class APA102Controller : public CLEDController {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() { mSPI.writeWord(0); mSPI.writeWord(0); }
	void endBoundary(int nLeds) { int nBytes = (nLeds/32); do { mSPI.writeByte(0xFF); mSPI.writeByte(0x00); mSPI.writeByte(0x00); mSPI.writeByte(0x00); } while(nBytes--); }

	inline void writeLed(uint8_t b0, uint8_t b1, uint8_t b2) __attribute__((always_inline)) {
		mSPI.writeByte(0xFF); mSPI.writeByte(b0); mSPI.writeByte(b1); mSPI.writeByte(b2);
	}

public:
	APA102Controller() {}

	virtual void init() {
		mSPI.init();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0,0,0), nLeds, CRGB(0,0,0));
	}

protected:

	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(data, nLeds, scale, getDither());

		mSPI.select();

		startBoundary();
		for(int i = 0; i < nLeds; i++) {
			uint8_t b = pixels.loadAndScale0();
			mSPI.writeWord(0xFF00 | b);
			uint16_t w = pixels.loadAndScale1() << 8;
			w |= pixels.loadAndScale2();
			mSPI.writeWord(w);
			pixels.stepDithering();
		}
		endBoundary(nLeds);

		mSPI.waitFully();
		mSPI.release();
	}

	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(data, nLeds, scale, getDither());

		mSPI.select();

		startBoundary();
		for(int i = 0; i < nLeds; i++) {
			uint16_t b = 0xFF00 | (uint16_t)pixels.loadAndScale0();
			mSPI.writeWord(b);
			uint16_t w = pixels.loadAndScale1() << 8;
			w |= pixels.loadAndScale2();
			mSPI.writeWord(w);
			pixels.advanceData();
			pixels.stepDithering();
		}
		endBoundary(nLeds);
		mSPI.waitFully();
		mSPI.release();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(data, nLeds,, scale, getDither());

		mSPI.select();

		startBoundary();
		for(int i = 0; i < nLeds; i++) {
			mSPI.writeByte(0xFF);
			uint8_t b = pixels.loadAndScale0(); mSPI.writeByte(b);
			b = pixels.loadAndScale1(); mSPI.writeByte(b);
			b = pixels.loadAndScale2(); mSPI.writeByte(b);
			pixels.advanceData();
			pixels.stepDithering();
		}
		endBoundary(nLeds);
		mSPI.waitFully();
		mSPI.release();
	}
#endif
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
class P9813Controller : public CLEDController {
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

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0,0,0), nLeds, CRGB(0,0,0));
	}

protected:

	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(data, nLeds, scale, getDither());

		mSPI.select();

		writeBoundary();
		while(nLeds--) {
			writeLed(pixels.loadAndScale0(), pixels.loadAndScale1(), pixels.loadAndScale2());
			pixels.stepDithering();
		}
		writeBoundary();

		mSPI.waitFully();
		mSPI.release();
	}

	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(data, nLeds, scale, getDither());

		mSPI.select();

		writeBoundary();
		for(int i = 0; i < nLeds; i++) {
			writeLed(pixels.loadAndScale0(), pixels.loadAndScale1(), pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
		writeBoundary();
		mSPI.waitFully();

		mSPI.release();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(data, nLeds,, scale, getDither());

		mSPI.select();

		writeBoundary();
		for(int i = 0; i < nLeds; i++) {
			writeLed(pixels.loadAndScale0(), pixels.loadAndScale1(), pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
		writeBoundary();
		mSPI.waitFully();

		mSPI.release();
	}
#endif
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

protected:

	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		mSPI.template writePixels<FLAG_START_BIT, DATA_NOP, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
		writeHeader();
	}

	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		// writeHeader();
		mSPI.template writePixels<FLAG_START_BIT, DATA_NOP, RGB_ORDER>( PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
		writeHeader();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *data, int nLeds, CRGB scale) {
		mSPI.writeBytesValue(0, 6);
		mSPI.template writeBit<0>(0);
		mSPI.template writeBit<0>(0);

		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		mSPI.template writePixels<FLAG_START_BIT, DATA_NOP, RGB_ORDER>(PixelController<RGB_ORDER>(data, nLeds, scale, getDither()));
	}
#endif
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

// WS2811@800khz 2 clocks, 5 clocks, 3 clocks
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2812Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 4 * FMUL, 3 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 10 * FMUL, 6 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 12 * FMUL, 4 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 4 * FMUL, 4 * FMUL, RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 3 * FMUL, 4 * FMUL, RGB_ORDER> {};

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

#else
// GW6205@400khz - 800ns, 800ns, 800ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller400Khz : public ClocklessController<DATA_PIN, NS(800), NS(800), NS(800), RGB_ORDER, 4> {};
#if NO_TIME(800, 800, 800)
#warning "Not enough clock cycles available for the GW6205@400khz"
#endif

// GW6205@400khz - 400ns, 400ns, 400ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller800Khz : public ClocklessController<DATA_PIN, NS(400), NS(400), NS(400), RGB_ORDER, 4> {};
#if NO_TIME(400, 400, 400)
#warning "Not enough clock cycles available for the GW6205@400khz"
#endif

// UCS1903 - 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, NS(500), NS(1500), NS(500), RGB_ORDER> {};
#if NO_TIME(500, 1500, 500)
#warning "Not enough clock cycles available for the UCS1903@400khz"
#endif

// UCS1903B - 400ns, 450ns, 450ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public ClocklessController<DATA_PIN, NS(400), NS(450), NS(450), RGB_ORDER> {};
#if NO_TIME(400, 450, 450)
#warning "Not enough clock cycles available for the UCS1903B@800khz"
#endif

// UCS1904 - 400ns, 400ns, 450ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public ClocklessController<DATA_PIN, NS(400), NS(400), NS(450), RGB_ORDER> {};
#if NO_TIME(400, 400, 450)
#warning "Not enough clock cycles available for the UCS1904@800khz"
#endif

// TM1809 - 350ns, 350ns, 550ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, NS(350), NS(350), NS(450), RGB_ORDER> {};
#if NO_TIME(350, 350, 550)
#warning "Not enough clock cycles available for the TM1809"
#endif

// WS2811 - 320ns, 320ns, 640ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, NS(320), NS(320), NS(640), RGB_ORDER> {};
#if NO_TIME(320, 320, 640)
#warning "Not enough clock cycles available for the WS2811 (800khz)"
#endif

// WS2812 - 250ns, 625ns, 375ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2812Controller800Khz : public ClocklessController<DATA_PIN, NS(250), NS(625), NS(375), RGB_ORDER> {};
#if NO_TIME(250, 625, 375)
#warning "Not enough clock cycles available for the WS2812 (800khz)"
#endif

// WS2811@400khz - 800ns, 800ns, 900ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, NS(800), NS(800), NS(900), RGB_ORDER> {};
#if NO_TIME(800, 800, 900)
#warning "Not enough clock cycles available for the WS2811 (400Khz)"
#endif

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, NS(700), NS(1100), NS(700), RGB_ORDER> {};
#if NO_TIME(750, 750, 750)
#warning "Not enough clock cycles available for the TM1803"
#endif

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller800Khz : public ClocklessController<DATA_PIN, NS(340), NS(340), NS(550), RGB_ORDER, 0, true, 500> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller1600Khz : public ClocklessController<DATA_PIN, NS(100), NS(300), NS(200), RGB_ORDER, 0, true, 500> {};
#if NO_TIME(100, 300, 200)
#warning "Not enough clock cycles available for TM1829@1.6Mhz"
#endif

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz : public ClocklessController<DATA_PIN, NS(200), NS(400), NS(200), RGB_ORDER, 4> {};
#if NO_TIME(200,400,200)
#warning "Not enough clock cycles for LPD1886"
#endif

#endif
///@}

#endif
///@}
FASTLED_NAMESPACE_END

#endif
