#pragma once

/// @file generic_software_spi.h
/// Generic cross-platform software SPI (bit-banging) implementation
///
/// @note Historical note: This class was previously named AVRSoftwareSPIOutput and lived in
///       src/fastspi_bitbang.h. It was renamed to GenericSoftwareSPIOutput and moved here to
///       better reflect its cross-platform nature. The old AVRSoftwareSPIOutput name was
///       misleading as this implementation works on all platforms, not just AVR.

#ifndef __INC_PLATFORMS_SHARED_SPI_BITBANG_GENERIC_SOFTWARE_SPI_H
#define __INC_PLATFORMS_SHARED_SPI_BITBANG_GENERIC_SOFTWARE_SPI_H

#include "fl/fastled.h"

#include "fastled_delay.h"
#include "fl/force_inline.h"
#include "fl/int.h"
#include "fl/fastpin.h"
#include "pixel_controller.h"
#include "fastspi_types.h"
namespace fl {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Generic software SPI (bit-banging) implementation that works on all platforms
/// Includes aggressive optimizations for when the clock and data pin are on the same port.
/// @tparam DATA_PIN pin number of the SPI data pin.
/// @tparam CLOCK_PIN pin number of the SPI clock pin.
/// @tparam SPI_SPEED speed of the bus. Determines the delay times between pin writes.
/// @note This is a cross-platform software SPI implementation that works on any platform.
/// @note Previously named AVRSoftwareSPIOutput (legacy name from when FastLED was AVR-focused)
/// @todo Replace the select pin definition with a set of pins, to allow using mux hardware for routing in the future.
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, fl::u32 SPI_SPEED>
class GenericSoftwareSPIOutput {
	// The data types for pointers to the pin port - typedef'd here from the ::Pin definition because on AVR these
	// are pointers to 8 bit values, while on ARM they are 32 bit
	typedef typename fl::FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename fl::FastPin<CLOCK_PIN>::port_ptr_t clock_ptr_t;

	// The data type for what's at a pin's port - typedef'd here from the Pin definition because on avr the ports
	// are 8 bits wide while on arm they are 32.
	typedef typename fl::FastPin<DATA_PIN>::port_t data_t;
	typedef typename fl::FastPin<CLOCK_PIN>::port_t clock_t;
	Selectable 	*m_pSelect;  ///< SPI chip select

public:
	/// Default constructor
	GenericSoftwareSPIOutput() { m_pSelect = nullptr; }
	/// Constructor with selectable for SPI chip select
	GenericSoftwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

	/// Set the pointer for the SPI chip select
	/// @param pSelect pointer to chip select control
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	/// Set the clock/data pins to output and make sure the chip select is released.
	void init() {
		// set the pins to output and make sure the select is released (which apparently means hi?  This is a bit
		// confusing to me)
		fl::FastPin<DATA_PIN>::setOutput();
		fl::FastPin<CLOCK_PIN>::setOutput();
		release();
	}

	/// Stop the SPI output.
	/// Pretty much a NOP with software, as there's no registers to kick
	static void stop() { }

	/// Wait until the SPI subsystem is ready for more data to write.
	/// A NOP when bitbanging.
	static void wait() __attribute__((always_inline)) { }
	/// @copydoc GenericSoftwareSPIOutput::wait()
	static void waitFully() __attribute__((always_inline)) { wait(); }

	/// Write a single byte over SPI without waiting.
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { writeByte(b); }
	/// Write a single byte over SPI and wait afterwards.
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { writeByte(b); wait(); }

	/// Write a word (two bytes) over SPI.
	static void writeWord(u16 w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	/// Write a single byte over SPI.
	/// Naive implelentation, simply calls writeBit() on the 8 bits in the byte.
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

private:
	/// writeByte() implementation with data/clock registers passed in.
	static void writeByte(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin)  {
		writeBit<7>(b, clockpin, datapin);
		writeBit<6>(b, clockpin, datapin);
		writeBit<5>(b, clockpin, datapin);
		writeBit<4>(b, clockpin, datapin);
		writeBit<3>(b, clockpin, datapin);
		writeBit<2>(b, clockpin, datapin);
		writeBit<1>(b, clockpin, datapin);
		writeBit<0>(b, clockpin, datapin);
	}

	/// writeByte() implementation with the data register passed in and prebaked values for data hi w/clock hi and
	/// low and data lo w/clock hi and lo.  This is to be used when clock and data are on the same GPIO register,
	/// can get close to getting a bit out the door in 2 clock cycles!
	static void writeByte(uint8_t b, data_ptr_t datapin,
						  data_t hival, data_t loval,
						  clock_t hiclock, clock_t loclock) {
		writeBit<7>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, datapin, hival, loval, hiclock, loclock);
	}

	/// writeByte() implementation with not just registers passed in, but pre-baked values for said registers for
	/// data hi/lo and clock hi/lo values.
	/// @note Weird things will happen if this method is called in cases where
	/// the data and clock pins are on the same port!  Don't do that!
	static void writeByte(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin,
						  data_t hival, data_t loval,
						  clock_t hiclock, clock_t loclock) {
		writeBit<7>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, clockpin, datapin, hival, loval, hiclock, loclock);
	}

public:

#if defined(FASTLED_TEENSY4)
	#define DELAY_NS (1000 / (SPI_SPEED/1000000))
	#define CLOCK_HI_DELAY do { delayNanoseconds((DELAY_NS/4)); } while(0);
	#define CLOCK_LO_DELAY do { delayNanoseconds((DELAY_NS/4)); } while(0);
#else
	/// We want to make sure that the clock pulse is held high for a minimum of 35 ns.
	#define MIN_DELAY ((NS(35)>3) ? (NS(35) - 3) : 1)

	/// Delay for the clock signal 'high' period
	#define CLOCK_HI_DELAY do { delaycycles<MIN_DELAY>(); delaycycles<((SPI_SPEED > 10) ? (((SPI_SPEED-6) / 2) - MIN_DELAY) : (SPI_SPEED))>(); } while(0);
	/// Delay for the clock signal 'low' period
	#define CLOCK_LO_DELAY do { delaycycles<((SPI_SPEED > 10) ? ((SPI_SPEED-6) / 2) : (SPI_SPEED))>(); } while(0);
#endif

	/// Write the BIT'th bit out via SPI, setting the data pin then strobing the clock
	/// @tparam BIT the bit index in the byte
	/// @param b the byte to read the bit from
	template <uint8_t BIT> __attribute__((always_inline, hot)) inline static void writeBit(uint8_t b) {
		//cli();
		if(b & (1 << BIT)) {
			fl::FastPin<DATA_PIN>::hi();
#ifdef ESP32
			// try to ensure we never have adjacent write opcodes to the same register
			fl::FastPin<CLOCK_PIN>::lo();
			fl::FastPin<CLOCK_PIN>::hi(); CLOCK_HI_DELAY;
			fl::FastPin<CLOCK_PIN>::toggle(); CLOCK_LO_DELAY;
#else
			fl::FastPin<CLOCK_PIN>::hi(); CLOCK_HI_DELAY;
			fl::FastPin<CLOCK_PIN>::lo(); CLOCK_LO_DELAY;
#endif
		} else {
			fl::FastPin<DATA_PIN>::lo();
			fl::FastPin<CLOCK_PIN>::hi(); CLOCK_HI_DELAY;
#ifdef ESP32
			// try to ensure we never have adjacent write opcodes to the same register
			fl::FastPin<CLOCK_PIN>::toggle(); CLOCK_HI_DELAY;
#else
			fl::FastPin<CLOCK_PIN>::lo(); CLOCK_LO_DELAY;
#endif
		}
		//sei();
	}

private:
	/// Write the BIT'th bit out via SPI, setting the data pin then strobing the clock, using the passed in pin registers to accelerate access if needed
	template <uint8_t BIT> FASTLED_FORCE_INLINE static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin) {
		if(b & (1 << BIT)) {
			fl::FastPin<DATA_PIN>::hi(datapin);
			fl::FastPin<CLOCK_PIN>::hi(clockpin); CLOCK_HI_DELAY;
			fl::FastPin<CLOCK_PIN>::lo(clockpin); CLOCK_LO_DELAY;
		} else {
			fl::FastPin<DATA_PIN>::lo(datapin);
			fl::FastPin<CLOCK_PIN>::hi(clockpin); CLOCK_HI_DELAY;
			fl::FastPin<CLOCK_PIN>::lo(clockpin); CLOCK_LO_DELAY;
		}

	}

	/// The version of writeBit() to use when clock and data are on separate pins with precomputed values for setting
	/// the clock and data pins
	template <uint8_t BIT> FASTLED_FORCE_INLINE static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin,
													data_t hival, data_t loval, clock_t hiclock, clock_t loclock) {
		// // only need to explicitly set clock hi if clock and data are on different ports
		if(b & (1 << BIT)) {
			fl::FastPin<DATA_PIN>::fastset(datapin, hival);
			fl::FastPin<CLOCK_PIN>::fastset(clockpin, hiclock); CLOCK_HI_DELAY;
			fl::FastPin<CLOCK_PIN>::fastset(clockpin, loclock); CLOCK_LO_DELAY;
		} else {
			// FL_NOP;
			fl::FastPin<DATA_PIN>::fastset(datapin, loval);
			fl::FastPin<CLOCK_PIN>::fastset(clockpin, hiclock); CLOCK_HI_DELAY;
			fl::FastPin<CLOCK_PIN>::fastset(clockpin, loclock); CLOCK_LO_DELAY;
		}
	}

	/// The version of writeBit() to use when clock and data are on the same port with precomputed values for the various
	/// combinations
	template <uint8_t BIT> FASTLED_FORCE_INLINE static void writeBit(uint8_t b, data_ptr_t clockdatapin,
													data_t datahiclockhi, data_t dataloclockhi,
													data_t datahiclocklo, data_t dataloclocklo) {
#if 0
		writeBit<BIT>(b);
#else
		if(b & (1 << BIT)) {
			fl::FastPin<DATA_PIN>::fastset(clockdatapin, datahiclocklo);
			fl::FastPin<DATA_PIN>::fastset(clockdatapin, datahiclockhi); CLOCK_HI_DELAY;
			fl::FastPin<DATA_PIN>::fastset(clockdatapin, datahiclocklo); CLOCK_LO_DELAY;
		} else {
			// FL_NOP;
			fl::FastPin<DATA_PIN>::fastset(clockdatapin, dataloclocklo);
			fl::FastPin<DATA_PIN>::fastset(clockdatapin, dataloclockhi); CLOCK_HI_DELAY;
			fl::FastPin<DATA_PIN>::fastset(clockdatapin, dataloclocklo); CLOCK_LO_DELAY;
		}
#endif
	}

public:

	/// Select the SPI output (chip select)
	/// @todo Research whether this really means 'hi' or 'lo'.
	/// @par
	/// @todo Move select responsibility out of the SPI classes entirely,
	///       make it up to the caller to remember to lock/select the line?
	void select() { if(m_pSelect != nullptr) { m_pSelect->select(); } } // fl::FastPin<SELECT_PIN>::hi(); }

	/// Release the SPI chip select line
	void release() { if(m_pSelect != nullptr) { m_pSelect->release(); } } // fl::FastPin<SELECT_PIN>::lo(); }

	void endTransaction() {
		waitFully();
		release();
	}

	/// Write multiple bytes of the given value over SPI.
	/// Useful for quickly flushing, say, a line of 0's down the line.
	/// @param value the value to write to the bus
	/// @param len how many copies of the value to write
	void writeBytesValue(uint8_t value, int len) {
		select();
		writeBytesValueRaw(value, len);
		release();
	}

	/// Write multiple bytes of the given value over SPI, without selecting the interface.
	/// @copydetails GenericSoftwareSPIOutput::writeBytesValue(uint8_t, int)
	static void writeBytesValueRaw(uint8_t value, int len) {
#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		// TODO: Weird things may happen if software bitbanging SPI output and other pins on the output reigsters are being twiddled.  Need
		// to allow specifying whether or not exclusive i/o access is allowed during this process, and if i/o access is not allowed fall
		// back to the degenerative code below
		while(len--) {
			writeByte(value);
		}
#else
		FASTLED_REGISTER data_ptr_t datapin = fl::FastPin<DATA_PIN>::port();

		if(fl::FastPin<DATA_PIN>::port() != fl::FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			FASTLED_REGISTER clock_ptr_t clockpin = fl::FastPin<CLOCK_PIN>::port();
			FASTLED_REGISTER data_t datahi = fl::FastPin<DATA_PIN>::hival();
			FASTLED_REGISTER data_t datalo = fl::FastPin<DATA_PIN>::loval();
			FASTLED_REGISTER clock_t clockhi = fl::FastPin<CLOCK_PIN>::hival();
			FASTLED_REGISTER clock_t clocklo = fl::FastPin<CLOCK_PIN>::loval();
			while(len--) {
				writeByte(value, clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins
			FASTLED_REGISTER data_t datahi_clockhi = fl::FastPin<DATA_PIN>::hival() | fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datalo_clockhi = fl::FastPin<DATA_PIN>::loval() | fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datahi_clocklo = fl::FastPin<DATA_PIN>::hival() & ~fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datalo_clocklo = fl::FastPin<DATA_PIN>::loval() & ~fl::FastPin<CLOCK_PIN>::mask();

			while(len--) {
				writeByte(value, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
#endif
	}

	/// Write an array of data to the SPI interface.
	/// @tparam D Per-byte modifier class, e.g. ::DATA_NOP
	/// @param data pointer to data to write
	/// @param len number of bytes to write
	/// @todo Need to type this better so that explicit casts into the call aren't required.
	template <class D> void writeBytes(FASTLED_REGISTER uint8_t *data, int len) {
		select();
#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		uint8_t *end = data + len;
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
#else
		FASTLED_REGISTER clock_ptr_t clockpin = fl::FastPin<CLOCK_PIN>::port();
		FASTLED_REGISTER data_ptr_t datapin = fl::FastPin<DATA_PIN>::port();

		if(fl::FastPin<DATA_PIN>::port() != fl::FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			FASTLED_REGISTER data_t datahi = fl::FastPin<DATA_PIN>::hival();
			FASTLED_REGISTER data_t datalo = fl::FastPin<DATA_PIN>::loval();
			FASTLED_REGISTER clock_t clockhi = fl::FastPin<CLOCK_PIN>::hival();
			FASTLED_REGISTER clock_t clocklo = fl::FastPin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) {
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// fl::FastPin<CLOCK_PIN>::hi();
			// If data and clock are on the same port then we can combine setting the data and clock pins
			FASTLED_REGISTER data_t datahi_clockhi = fl::FastPin<DATA_PIN>::hival() | fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datalo_clockhi = fl::FastPin<DATA_PIN>::loval() | fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datahi_clocklo = fl::FastPin<DATA_PIN>::hival() & ~fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datalo_clocklo = fl::FastPin<DATA_PIN>::loval() & ~fl::FastPin<CLOCK_PIN>::mask();

			uint8_t *end = data + len;

			while(data != end) {
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
			// fl::FastPin<CLOCK_PIN>::lo();
		}
#endif
		D::postBlock(len, this);
		release();
	}

	/// Write an array of data to the SPI interface.
	/// @param data pointer to data to write
	/// @param len number of bytes to write
	void writeBytes(FASTLED_REGISTER uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	/// Finalize transmission (no-op for software SPI)
	/// This method exists for compatibility with hardware SPI implementations
	/// that may need to flush buffers or perform post-transmission operations
	static void finalizeTransmission() { }

	/// Write LED pixel data to the SPI interface.
	/// Data is written in groups of three, re-ordered per the RGB_ORDER.
	/// @tparam FLAGS Option flags, such as ::FLAG_START_BIT
	/// @tparam D Per-byte modifier class, e.g. ::DATA_NOP
	/// @tparam RGB_ORDER the rgb ordering for the LED data (e.g. what order red, green, and blue data is written out in)
	/// @param pixels a ::PixelController with the LED data and modifier options
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER>  __attribute__((noinline)) void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) {
		FASTLED_UNUSED(context);
		select();
		int len = pixels.mLen;

#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		// If interrupts or other things may be generating output while we're working on things, then we need
		// to use this block
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
#else
		// If we can guaruntee that no one else will be writing data while we are running (namely, changing the values of the PORT/PDOR pins)
		// then we can use a bunch of optimizations in here
		FASTLED_REGISTER data_ptr_t datapin = fl::FastPin<DATA_PIN>::port();

		if(fl::FastPin<DATA_PIN>::port() != fl::FastPin<CLOCK_PIN>::port()) {
			FASTLED_REGISTER clock_ptr_t clockpin = fl::FastPin<CLOCK_PIN>::port();
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			FASTLED_REGISTER data_t datahi = fl::FastPin<DATA_PIN>::hival();
			FASTLED_REGISTER data_t datalo = fl::FastPin<DATA_PIN>::loval();
			FASTLED_REGISTER clock_t clockhi = fl::FastPin<CLOCK_PIN>::hival();
			FASTLED_REGISTER clock_t clocklo = fl::FastPin<CLOCK_PIN>::loval();

			while(pixels.has(1)) {
				if(FLAGS & FLAG_START_BIT) {
					writeBit<0>(1, clockpin, datapin, datahi, datalo, clockhi, clocklo);
				}
				writeByte(D::adjust(pixels.loadAndScale0()), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(pixels.loadAndScale1()), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(pixels.loadAndScale2()), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				pixels.advanceData();
				pixels.stepDithering();
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins
			FASTLED_REGISTER data_t datahi_clockhi = fl::FastPin<DATA_PIN>::hival() | fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datalo_clockhi = fl::FastPin<DATA_PIN>::loval() | fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datahi_clocklo = fl::FastPin<DATA_PIN>::hival() & ~fl::FastPin<CLOCK_PIN>::mask();
			FASTLED_REGISTER data_t datalo_clocklo = fl::FastPin<DATA_PIN>::loval() & ~fl::FastPin<CLOCK_PIN>::mask();

			while(pixels.has(1)) {
				if(FLAGS & FLAG_START_BIT) {
					writeBit<0>(1, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				}
				writeByte(D::adjust(pixels.loadAndScale0()), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(pixels.loadAndScale1()), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(pixels.loadAndScale2()), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				pixels.advanceData();
				pixels.stepDithering();
			}
		}
#endif
		D::postBlock(len, this);
		release();
	}
};
}  // namespace fl
#endif
