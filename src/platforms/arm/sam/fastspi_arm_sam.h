// IWYU pragma: private

#ifndef __INC_FASTSPI_ARM_SAM_H
#define __INC_FASTSPI_ARM_SAM_H

// Includes must come before all namespace declarations
#include "fastspi_types.h"
#include "platforms/arm/sam/is_sam.h"
#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAM)
#endif

#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
#include <SPI.h>
#include <wiring_private.h>
// IWYU pragma: end_keep
#include "fl/system/pin.h"  // For PinMode, PinValue enums
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
#endif

#if defined(FL_IS_SAM)
namespace fl {
#define m_SPI ((Spi*)SPI0)

template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SAMHardwareSPIOutput {
	Selectable *mPSelect;

	static inline void waitForEmpty() { while ((m_SPI->SPI_SR & SPI_SR_TDRE) == 0); }

	void enableConfig() { m_SPI->SPI_WPMR &= ~SPI_WPMR_WPEN; }
	void disableConfig() { m_SPI->SPI_WPMR |= SPI_WPMR_WPEN; }

	void enableSPI() { m_SPI->SPI_CR = SPI_CR_SPIEN; }
	void disableSPI() { m_SPI->SPI_CR = SPI_CR_SPIDIS; }
	void resetSPI() { m_SPI->SPI_CR = SPI_CR_SWRST; }

	static inline void readyTransferBits(FASTLED_REGISTER u32 bits) FL_NOEXCEPT {
		bits -= 8;
		// don't change the number of transfer bits while data is still being transferred from TDR to the shift register
		waitForEmpty();
		m_SPI->SPI_CSR[0] = SPI_CSR_NCPHA | SPI_CSR_CSAAT | (bits << SPI_CSR_BITS_Pos) | SPI_CSR_DLYBCT(1) | SPI_CSR_SCBR(_SPI_CLOCK_DIVIDER);
	}

	template<int BITS> static inline void writeBits(u16 w) FL_NOEXCEPT {
		waitForEmpty();
		m_SPI->SPI_TDR = (u32)w | SPI_PCS(0);
	}

public:
	SAMHardwareSPIOutput() { mPSelect = nullptr; }
	SAMHardwareSPIOutput(Selectable *pSelect) { mPSelect = pSelect; }

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() FL_NOEXCEPT {
		// m_SPI = SPI0;

		// set the output pins master out, master in, clock.  Note doing this here because I still don't
		// know how I want to expose this type of functionality in FastPin.
		PIO_Configure(PIOA, PIO_PERIPH_A, FastPin<_DATA_PIN>::mask(), PIO_DEFAULT) FL_NOEXCEPT;
		PIO_Configure(PIOA, PIO_PERIPH_A, FastPin<_DATA_PIN-1>::mask(), PIO_DEFAULT) FL_NOEXCEPT;
		PIO_Configure(PIOA, PIO_PERIPH_A, FastPin<_CLOCK_PIN>::mask(), PIO_DEFAULT) FL_NOEXCEPT;

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
	void inline select() __attribute__((always_inline)) { if(mPSelect != nullptr) { mPSelect->select(); } }

	// release the CS select
	void inline release() __attribute__((always_inline)) { if(mPSelect != nullptr) { mPSelect->release(); } }

	void endTransaction() FL_NOEXCEPT {
		waitFully();
		release();
	}

	// wait until all queued up data has been written
	void waitFully() { while((m_SPI->SPI_SR & SPI_SR_TXEMPTY) == 0); }

	// write a byte out via SPI (returns immediately on writing register)
	static void writeByte(u8 b) FL_NOEXCEPT {
		writeBits<8>(b);
	}

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(u16 w) FL_NOEXCEPT {
		writeBits<16>(w);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(u8 value, int len) FL_NOEXCEPT {
		while(len--) { writeByte(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(u8 value, int len) FL_NOEXCEPT {
		select(); writeBytesValueRaw(value, len); release();
	}

	template <class D> void writeBytes(FASTLED_REGISTER u8 *data, int len) FL_NOEXCEPT {
		u8 *end = data + len;
		select();
		// could be optimized to write 16bit words out instead of 8bit bytes
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		waitFully();
		release();
	}

	void writeBytes(FASTLED_REGISTER u8 *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	// not the most efficient mechanism in the world - but should be enough for sm16716 and friends
	template <u8 BIT> inline void writeBit(u8 b) FL_NOEXCEPT {
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
	template <u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NOEXCEPT {
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
#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)

namespace fl {

template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SAMDHardwareSPIOutput {
private:
	Sercom* mSPI;
	Selectable *mPSelect;
	u8 mSercomNum;
	bool mInitialized;

	static inline void waitForEmpty(Sercom* spi) FL_NOEXCEPT {
		while (!spi->SPI.INTFLAG.bit.DRE);
	}

	static inline void waitForComplete(Sercom* spi) FL_NOEXCEPT {
		while (!spi->SPI.INTFLAG.bit.TXC);
	}

public:
	SAMDHardwareSPIOutput() : mSPI(nullptr), mPSelect(nullptr), mSercomNum(0), mInitialized(false) {}
	SAMDHardwareSPIOutput(Selectable *pSelect) : mSPI(nullptr), mPSelect(pSelect), mSercomNum(0), mInitialized(false) {}

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { mPSelect = pSelect; }

	// Helper to get SERCOM instance from Arduino's SPI object
	// On SAMD, the default SPI object uses a specific SERCOM
	// We'll use Arduino's built-in SPI functionality if available
	void init() FL_NOEXCEPT {
		if (mInitialized) {
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

		#if defined(FL_IS_SAMD51)
		// SAMD51 - Arduino core typically uses SERCOM1 or specific board SERCOM
		// Use the SPI peripheral that Arduino configured
		mSPI = &(::SPI);
		#elif defined(FL_IS_SAMD21)
		// SAMD21 - Arduino core typically uses SERCOM4
		mSPI = &(::SPI);
		#endif

		// Configure SPI settings
		// Clock divider maps to SPI frequency
		// Default Arduino SPI uses 4MHz, we can adjust based on _SPI_CLOCK_DIVIDER
		u32 clock_hz = F_CPU / _SPI_CLOCK_DIVIDER;
		if (clock_hz > 24000000) clock_hz = 24000000;  // Max 24MHz for safety

		::SPI.beginTransaction(SPISettings(clock_hz, MSBFIRST, SPI_MODE0));
		::SPI.endTransaction();

		mInitialized = true;
	}

	// latch the CS select
	void inline select() FL_NOEXCEPT __attribute__((always_inline)) {
		if(mPSelect != nullptr) {
			mPSelect->select();
		}
		if (mInitialized) {
			u32 clock_hz = F_CPU / _SPI_CLOCK_DIVIDER;
			if (clock_hz > 24000000) clock_hz = 24000000;
			::SPI.beginTransaction(SPISettings(clock_hz, MSBFIRST, SPI_MODE0));
		}
	}

	// release the CS select
	void inline release() FL_NOEXCEPT __attribute__((always_inline)) {
		if (mInitialized) {
			::SPI.endTransaction();
		}
		if(mPSelect != nullptr) {
			mPSelect->release();
		}
	}

	void endTransaction() FL_NOEXCEPT {
		waitFully();
		release();
	}

	// wait until all queued up data has been written
	void waitFully() FL_NOEXCEPT {
		// Arduino SPI is blocking, so no need to wait
	}

	// write a byte out via SPI (returns immediately on writing register)
	static void writeByte(u8 b) FL_NOEXCEPT {
		::SPI.transfer(b);
	}

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(u16 w) FL_NOEXCEPT {
		::SPI.transfer16(w);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(u8 value, int len) FL_NOEXCEPT {
		while(len--) { writeByte(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(u8 value, int len) FL_NOEXCEPT {
		select();
		writeBytesValueRaw(value, len);
		release();
	}

	template <class D> void writeBytes(FASTLED_REGISTER u8 *data, int len) FL_NOEXCEPT {
		u8 *end = data + len;
		select();
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		waitFully();
		release();
	}

	void writeBytes(FASTLED_REGISTER u8 *data, int len) FL_NOEXCEPT {
		writeBytes<DATA_NOP>(data, len);
	}

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <u8 BIT> inline void writeBit(u8 b) FL_NOEXCEPT {
		// For bit-banging, we need to temporarily disable SPI and use GPIO
		::SPI.endTransaction();

		pinMode(_DATA_PIN, PinMode::Output);
		pinMode(_CLOCK_PIN, PinMode::Output);

		if(b & (1 << BIT)) {
			digitalWrite(_DATA_PIN, PinValue::High);
		} else {
			digitalWrite(_DATA_PIN, PinValue::Low);
		}

		digitalWrite(_CLOCK_PIN, PinValue::High);
		digitalWrite(_CLOCK_PIN, PinValue::Low);

		// Re-initialize pins for SPI
		::SPI.begin();
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.
	template <u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NOEXCEPT {
		select();
		int len = pixels.mLen;

		if(FLAGS & FLAG_START_BIT) {
			// For chipsets that need a start bit (e.g., APA102)
			while(pixels.has(1)) {
				// Write 9 bits: 1 start bit + 8 data bits
				// Since we can't do 9-bit SPI easily, we'll use two bytes
				u16 word = (1<<8) | D::adjust(pixels.loadAndScale0());
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
#endif  // FL_IS_SAMD21 || FL_IS_SAMD51


FL_DISABLE_WARNING_POP

#endif
