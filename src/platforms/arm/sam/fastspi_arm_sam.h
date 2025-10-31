#ifndef __INC_FASTSPI_ARM_SAM_H
#define __INC_FASTSPI_ARM_SAM_H

// Includes must come before all namespace declarations
#include "fastspi_types.h"

#if defined(__SAM3X8E__)
#endif

#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
    defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#include <Arduino.h>
#include <SPI.h>
#include <wiring_private.h>
#endif

#if defined(__SAM3X8E__)
namespace fl {
#define m_SPI ((Spi*)SPI0)

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint32_t _SPI_CLOCK_DIVIDER>
class SAMHardwareSPIOutput {
	Selectable *m_pSelect;

	static inline void waitForEmpty() { while ((m_SPI->SPI_SR & SPI_SR_TDRE) == 0); }

	void enableConfig() { m_SPI->SPI_WPMR &= ~SPI_WPMR_WPEN; }
	void disableConfig() { m_SPI->SPI_WPMR |= SPI_WPMR_WPEN; }

	void enableSPI() { m_SPI->SPI_CR = SPI_CR_SPIEN; }
	void disableSPI() { m_SPI->SPI_CR = SPI_CR_SPIDIS; }
	void resetSPI() { m_SPI->SPI_CR = SPI_CR_SWRST; }

	static inline void readyTransferBits(FASTLED_REGISTER uint32_t bits) {
		bits -= 8;
		// don't change the number of transfer bits while data is still being transferred from TDR to the shift register
		waitForEmpty();
		m_SPI->SPI_CSR[0] = SPI_CSR_NCPHA | SPI_CSR_CSAAT | (bits << SPI_CSR_BITS_Pos) | SPI_CSR_DLYBCT(1) | SPI_CSR_SCBR(_SPI_CLOCK_DIVIDER);
	}

	template<int BITS> static inline void writeBits(uint16_t w) {
		waitForEmpty();
		m_SPI->SPI_TDR = (uint32_t)w | SPI_PCS(0);
	}

public:
	SAMHardwareSPIOutput() { m_pSelect = nullptr; }
	SAMHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() {
		// m_SPI = SPI0;

		// set the output pins master out, master in, clock.  Note doing this here because I still don't
		// know how I want to expose this type of functionality in FastPin.
		PIO_Configure(PIOA, PIO_PERIPH_A, FastPin<_DATA_PIN>::mask(), PIO_DEFAULT);
		PIO_Configure(PIOA, PIO_PERIPH_A, FastPin<_DATA_PIN-1>::mask(), PIO_DEFAULT);
		PIO_Configure(PIOA, PIO_PERIPH_A, FastPin<_CLOCK_PIN>::mask(), PIO_DEFAULT);

		release();

		// Configure the SPI clock, divider between 1-255
		// SCBR = _SPI_CLOCK_DIVIDER
		pmc_enable_periph_clk(ID_SPI0);
		disableSPI();

		// reset twice (what the sam code does, not sure why?)
		resetSPI();
		resetSPI();

		// Configure SPI as master, enable
		// Bits we want in MR: master, disable mode fault detection, variable peripheral select
		m_SPI->SPI_MR = SPI_MR_MSTR | SPI_MR_MODFDIS | SPI_MR_PS;

		enableSPI();

		// Send everything out in 8 bit chunks, other sizes appear to work, poorly...
		readyTransferBits(8);
	}

	// latch the CS select
	void inline select() __attribute__((always_inline)) { if(m_pSelect != nullptr) { m_pSelect->select(); } }

	// release the CS select
	void inline release() __attribute__((always_inline)) { if(m_pSelect != nullptr) { m_pSelect->release(); } }

	void endTransaction() {
		waitFully();
		release();
	}

	// wait until all queued up data has been written
	void waitFully() { while((m_SPI->SPI_SR & SPI_SR_TXEMPTY) == 0); }

	// write a byte out via SPI (returns immediately on writing register)
	static void writeByte(uint8_t b) {
		writeBits<8>(b);
	}

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(uint16_t w) {
		writeBits<16>(w);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(uint8_t value, int len) {
		select(); writeBytesValueRaw(value, len); release();
	}

	template <class D> void writeBytes(FASTLED_REGISTER uint8_t *data, int len) {
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

	void writeBytes(FASTLED_REGISTER uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	// not the most efficient mechanism in the world - but should be enough for sm16716 and friends
	template <uint8_t BIT> inline void writeBit(uint8_t b) {
		// need to wait for all exisiting data to go out the door, first
		waitFully();
		disableSPI();
		if(b & (1 << BIT)) {
			FastPin<_DATA_PIN>::hi();
		} else {
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
		enableSPI();
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) {
		select();
		int len = pixels.mLen;

		if(FLAGS & FLAG_START_BIT) {
			while(pixels.has(1)) {
				writeBits<9>((1<<8) | D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
				pixels.advanceData();
				pixels.stepDithering();
			}
		} else {
			while(pixels.has(1)) {
				writeByte(D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
				pixels.advanceData();
				pixels.stepDithering();
			}
		}
		D::postBlock(len);
		release();
	}

	/// Finalize transmission (no-op for SAM SPI)
	/// This method exists for compatibility with other SPI implementations
	/// that may need to flush buffers or perform post-transmission operations
	static void finalizeTransmission() { }
};

}  // namespace fl
#endif

// SAMD21/SAMD51 SERCOM-based SPI implementation
#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
    defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)

namespace fl {

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint32_t _SPI_CLOCK_DIVIDER>
class SAMDHardwareSPIOutput {
private:
	Sercom* m_SPI;
	Selectable *m_pSelect;
	uint8_t m_sercom_num;
	bool m_initialized;

	static inline void waitForEmpty(Sercom* spi) {
		while (!spi->SPI.INTFLAG.bit.DRE);
	}

	static inline void waitForComplete(Sercom* spi) {
		while (!spi->SPI.INTFLAG.bit.TXC);
	}

public:
	SAMDHardwareSPIOutput() : m_SPI(nullptr), m_pSelect(nullptr), m_sercom_num(0), m_initialized(false) {}
	SAMDHardwareSPIOutput(Selectable *pSelect) : m_SPI(nullptr), m_pSelect(pSelect), m_sercom_num(0), m_initialized(false) {}

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	// Helper to get SERCOM instance from Arduino's SPI object
	// On SAMD, the default SPI object uses a specific SERCOM
	// We'll use Arduino's built-in SPI functionality if available
	void init() {
		if (m_initialized) {
			return;
		}

		// Use Arduino's built-in SPI library for SAMD
		// This leverages the proper SERCOM already configured by the Arduino core
		::SPI.begin();

		// Get the SERCOM instance used by Arduino's SPI
		// Note: Different boards use different SERCOM units:
		// - Arduino Zero: SERCOM4
		// - Feather M0: SERCOM4
		// - Feather M4: SERCOM1
		// We rely on Arduino core's pin definitions

		#if defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
		    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
		// SAMD51 - Arduino core typically uses SERCOM1 or specific board SERCOM
		// Use the SPI peripheral that Arduino configured
		m_SPI = &(::SPI);
		#elif defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || \
		      defined(__SAMD21E17A__) || defined(__SAMD21E18A__)
		// SAMD21 - Arduino core typically uses SERCOM4
		m_SPI = &(::SPI);
		#endif

		// Configure SPI settings
		// Clock divider maps to SPI frequency
		// Default Arduino SPI uses 4MHz, we can adjust based on _SPI_CLOCK_DIVIDER
		uint32_t clock_hz = F_CPU / _SPI_CLOCK_DIVIDER;
		if (clock_hz > 24000000) clock_hz = 24000000;  // Max 24MHz for safety

		::SPI.beginTransaction(SPISettings(clock_hz, MSBFIRST, SPI_MODE0));
		::SPI.endTransaction();

		m_initialized = true;
	}

	// latch the CS select
	void inline select() __attribute__((always_inline)) {
		if(m_pSelect != nullptr) {
			m_pSelect->select();
		}
		if (m_initialized) {
			uint32_t clock_hz = F_CPU / _SPI_CLOCK_DIVIDER;
			if (clock_hz > 24000000) clock_hz = 24000000;
			::SPI.beginTransaction(SPISettings(clock_hz, MSBFIRST, SPI_MODE0));
		}
	}

	// release the CS select
	void inline release() __attribute__((always_inline)) {
		if (m_initialized) {
			::SPI.endTransaction();
		}
		if(m_pSelect != nullptr) {
			m_pSelect->release();
		}
	}

	void endTransaction() {
		waitFully();
		release();
	}

	// wait until all queued up data has been written
	void waitFully() {
		// Arduino SPI is blocking, so no need to wait
	}

	// write a byte out via SPI (returns immediately on writing register)
	static void writeByte(uint8_t b) {
		::SPI.transfer(b);
	}

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(uint16_t w) {
		::SPI.transfer16(w);
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

	template <class D> void writeBytes(FASTLED_REGISTER uint8_t *data, int len) {
		uint8_t *end = data + len;
		select();
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		waitFully();
		release();
	}

	void writeBytes(FASTLED_REGISTER uint8_t *data, int len) {
		writeBytes<DATA_NOP>(data, len);
	}

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline void writeBit(uint8_t b) {
		// For bit-banging, we need to temporarily disable SPI and use GPIO
		::SPI.endTransaction();

		pinMode(_DATA_PIN, OUTPUT);
		pinMode(_CLOCK_PIN, OUTPUT);

		if(b & (1 << BIT)) {
			digitalWrite(_DATA_PIN, HIGH);
		} else {
			digitalWrite(_DATA_PIN, LOW);
		}

		digitalWrite(_CLOCK_PIN, HIGH);
		digitalWrite(_CLOCK_PIN, LOW);

		// Re-initialize pins for SPI
		::SPI.begin();
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) {
		select();
		int len = pixels.mLen;

		if(FLAGS & FLAG_START_BIT) {
			// For chipsets that need a start bit (e.g., APA102)
			while(pixels.has(1)) {
				// Write 9 bits: 1 start bit + 8 data bits
				// Since we can't do 9-bit SPI easily, we'll use two bytes
				uint16_t word = (1<<8) | D::adjust(pixels.loadAndScale0());
				writeWord(word);
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
				pixels.advanceData();
				pixels.stepDithering();
			}
		} else {
			while(pixels.has(1)) {
				writeByte(D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
				pixels.advanceData();
				pixels.stepDithering();
			}
		}
		D::postBlock(len);
		release();
	}

	/// Finalize transmission (no-op for SAMD SPI using Arduino core)
	/// This method exists for compatibility with other SPI implementations
	static void finalizeTransmission() { }
};

}  // namespace fl
#endif  // SAMD21/SAMD51

#endif
