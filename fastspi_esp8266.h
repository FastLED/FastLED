#ifndef __INC_FASTSPI_ESP8266_H
#define __INC_FASTSPI_ESP8266_H

#include "FastLED.h"

#include "fastled_delay.h"

#include<SPI.h>

FASTLED_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// An attempt to use harware spi on ESP8266
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class ESP8266SPIOutput {
	Selectable 	*m_pSelect;

public:
	ESP8266SPIOutput() { m_pSelect = NULL; }
	ESP8266SPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		// set the pins to output and make sure the select is released (which apparently means hi?  This is a bit
		// confusing to me)
		SPI.begin();
		release();
	}

	// stop the SPI output.  Pretty much a NOP with software, as there's no registers to kick
	static void stop() { }

	// wait until the SPI subsystem is ready for more data to write.  A NOP when bitbanging
	static void wait() __attribute__((always_inline)) { }
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { writeByte(b); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { writeByte(b); wait(); }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	// naive writeByte implelentation, simply calls writeBit on the 8 bits in the byte.
	static void writeByte(uint8_t b) {
		SPI.transfer(b);
	}

public:

	// select the SPI output (TODO: research whether this really means hi or lo.  Alt TODO: move select responsibility out of the SPI classes
	// entirely, make it up to the caller to remember to lock/select the line?)
	void select() { 
		SPI.beginTransaction(SPISettings(3200000, MSBFIRST, SPI_MODE0));
		if(m_pSelect != NULL) { m_pSelect->select(); } 
	} 

	// release the SPI line
	void release() { 
		if(m_pSelect != NULL) { m_pSelect->release(); } 
		SPI.endTransaction();
	}

	// Write out len bytes of the given value out over SPI.  Useful for quickly flushing, say, a line of 0's down the line.
	void writeBytesValue(uint8_t value, int len) {
		select();
		writeBytesValueRaw(value, len);
		release();
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) {
			SPI.transfer(value); 
		}
	}

	// write a block of len uint8_ts out.  Need to type this better so that explicit casts into the call aren't required.
	// note that this template version takes a class parameter for a per-byte modifier to the data.
	template <class D> void writeBytes(register uint8_t *data, int len) {
		select();
		uint8_t *end = data + len;
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		release();
	}

	// default version of writing a block of data out to the SPI port, with no data modifications being made
	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline void writeBit(uint8_t b) {
		SPI.transfer(b);
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning of each grouping, as well as a class specifying a per
	// byte of data modification to be made.  (See DATA_NOP above)
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER>  __attribute__((noinline)) void writePixels(PixelController<RGB_ORDER> pixels) {
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
		release();
	}
};

FASTLED_NAMESPACE_END

#endif
