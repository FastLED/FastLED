#pragma once
#pragma message "ESP32 Hardware SPI support added"

FASTLED_NAMESPACE_BEGIN

#include <SPI.h>

/*
 * ESP32 Hardware SPI Driver
 *
 * Copyright (c) 2020 Nick Wallace
 * Derived from code for ESP8266 hardware SPI by Benoit Anastay.
 * 
 * This hardware SPI implementation can drive clocked LEDs from either the
 * VSPI or HSPI bus (aka SPI2 & SPI3). No support is provided for SPI1, because it is 
 * shared among devices and the cache for data (code) in the Flash as well as the PSRAM.
 *
 * To enable the hardware SPI driver, add the following line *before* including
 * FastLED.h:
 *
 * #define FASTLED_ALL_PINS_HARDWARE_SPI
 *
 * This driver uses the VSPI bus by default (GPIO 18, 19, 23, & 5). To use the 
 * HSPI bus (GPIO 14, 12, 13, & 15) add the following line *before* including
 * FastLED.h:
 * 
 * #define FASTLED_ESP32_SPI_BUS HSPI
 * 
 */
/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include<SPI.h>

// Conditional compilation for ESP32-S3 to utilize its flexible SPI capabilities
#if CONFIG_IDF_TARGET_ESP32S3
	#pragma message "Targeting ESP32S3, which has better SPI support. Configuring for flexible pin assignment."
	#undef FASTLED_ESP32_SPI_BUS
	// I *think* we have to "fake" being FSPI... there might be a better way to do this.
	// whatever the case, this "tricks" the pin assignment defines below into using DATA_PIN & CLOCK_PIN
	#define FASTLED_ESP32_SPI_BUS FSPI
#else // Configuration for other ESP32 variants
	#ifndef FASTLED_ESP32_SPI_BUS
	#pragma message "Setting ESP32 SPI bus to VSPI by default"
	#define FASTLED_ESP32_SPI_BUS VSPI
	#endif
#endif

#if FASTLED_ESP32_SPI_BUS == VSPI
    static int8_t spiClk = 18;
    static int8_t spiMiso = 19;
    static int8_t spiMosi = 23;
    static int8_t spiCs = 5;
#elif FASTLED_ESP32_SPI_BUS == HSPI
    static int8_t spiClk = 14;
    static int8_t spiMiso = 12;
    static int8_t spiMosi = 13;
    static int8_t spiCs = 15;
#elif FASTLED_ESP32_SPI_BUS == FSPI  // ESP32S2 can re-route to arbitrary pins
    #define spiMosi DATA_PIN
    #define spiClk CLOCK_PIN
    #define spiMiso -1
    #define spiCs -1
#endif

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class ESP32SPIOutput {
    SPIClass m_ledSPI;
	Selectable 	*m_pSelect;

public:
	// Verify that the pins are valid
	static_assert(FastPin<DATA_PIN>::validpin(), "Invalid data pin specified");
	static_assert(FastPin<CLOCK_PIN>::validpin(), "Invalid clock pin specified");

	ESP32SPIOutput() :
	  m_ledSPI(FASTLED_ESP32_SPI_BUS),
	  m_pSelect(nullptr) {}
	ESP32SPIOutput(Selectable *pSelect) :
	  m_ledSPI(FASTLED_ESP32_SPI_BUS),
	  m_pSelect(pSelect) {}
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		// set the pins to output and make sure the select is released (which apparently means hi?  This is a bit
		// confusing to me)
		m_ledSPI.begin(spiClk, spiMiso, spiMosi, spiCs);
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
	void writeByte(uint8_t b) {
		m_ledSPI.transfer(b);
	}

public:

	// select the SPI output (TODO: research whether this really means hi or lo.  Alt TODO: move select responsibility out of the SPI classes
	// entirely, make it up to the caller to remember to lock/select the line?)
	void select() { 
		m_ledSPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
		if(m_pSelect != NULL) { m_pSelect->select(); } 
	} 

	// release the SPI line
	void release() { 
		if(m_pSelect != NULL) { m_pSelect->release(); } 
		m_ledSPI.endTransaction();
	}

	// Write out len bytes of the given value out over m_ledSPI.  Useful for quickly flushing, say, a line of 0's down the line.
	void writeBytesValue(uint8_t value, int len) {
		select();
		writeBytesValueRaw(value, len);
		release();
	}

	void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) {
			m_ledSPI.transfer(value); 
		}
	}

	// write a block of len uint8_ts out.  Need to type this better so that explicit casts into the call aren't required.
	// note that this template version takes a class parameter for a per-byte modifier to the data.
	template <class D> void writeBytes(FASTLED_REGISTER uint8_t *data, int len) {
		select();
		uint8_t *end = data + len;
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		release();
	}

	// default version of writing a block of data out to the SPI port, with no data modifications being made
	void writeBytes(FASTLED_REGISTER uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline void writeBit(uint8_t b) {
		m_ledSPI.transfer(b);
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
