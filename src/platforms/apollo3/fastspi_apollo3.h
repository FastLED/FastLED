#ifndef __INC_FASTSPI_APOLLO3_H
#define __INC_FASTSPI_APOLLO3_H

// This is the implementation of fastspi for the Apollo3.
// It uses fastgpio instead of actual SPI, which means you can use it on all pins.
// It can run slightly faster than the default fastpin (bit banging).

#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_APOLLO3)

#define FASTLED_ALL_PINS_HARDWARE_SPI

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint32_t _SPI_CLOCK_DIVIDER>
class APOLLO3HardwareSPIOutput {
	Selectable *m_pSelect;

public:
	APOLLO3HardwareSPIOutput() { m_pSelect = NULL; }
	APOLLO3HardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	// initialize the pins for fastgpio
	void init() {
		FastPin<_CLOCK_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::lo();
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_DATA_PIN>::lo();
	}

	// latch the CS select
	void inline select() { /* TODO */ }

	// release the CS select
	void inline release() { /* TODO */ }

	// wait until all queued up data has been written
	static void waitFully() { /* TODO */ }

	// write a byte as bits
	static void writeByte(uint8_t b) {
		writeBit<7>(b);
		writeBit<6>(b);
		writeBit<5>(b);
		writeBit<4>(b);
		writeBit<3>(b);
		writeBit<2>(b);
		writeBit<1>(b);
		writeBit<0>(b);
	}

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(uint16_t w) {
		writeByte((uint8_t)((w >> 8) & 0xff));
		writeByte((uint8_t)(w & 0xff));
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(uint8_t value, int len) {
		select();
		writeBytesValueRaw(value, len);
		release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	template <class D> void writeBytes(REGISTER uint8_t *data, int len) {
		uint8_t *end = data + len;
		select();
		// could be optimized to write 16bit words out instead of 8bit bytes
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		waitFully();
		release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytes(REGISTER uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline static void writeBit(uint8_t b) {
		//waitFully();
		if(b & (1 << BIT)) {
			FastPin<_DATA_PIN>::hi();
		} else {
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		for (uint32_t d = (_SPI_CLOCK_DIVIDER >> 1); d > 0; d--) { __NOP(); }
		FastPin<_CLOCK_PIN>::lo();
		for (uint32_t d = (_SPI_CLOCK_DIVIDER >> 1); d > 0; d--) { __NOP(); }
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		select();

		int len = pixels.mLen;

		while(pixels.has(1)) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
			}
			writeByte(D::adjust(pixels.loadAndScale0()));
			writeByte(D::adjust(pixels.loadAndScale1()));
			writeByte(D::adjust(pixels.loadAndScale2()));

			pixels.advanceData();
			pixels.stepDithering();
		}
		D::postBlock(len);
		//waitFully();
		release();
	}

};

#endif

FASTLED_NAMESPACE_END

#endif
