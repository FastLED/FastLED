// IWYU pragma: private

#ifndef __INC_FASTSPI_ARM_MXRT1062_H
#define __INC_FASTSPI_ARM_MXRT1062_H

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)
// IWYU pragma: begin_keep
#include <SPI.h>
// IWYU pragma: end_keep
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_RATE, SPIClass & _SPIObject, int _SPI_INDEX>
class Teensy4HardwareSPIOutput {
	Selectable *mPSelect = nullptr;
	u32  mBitCount = 0;
	u32 mBitData = 0;
	inline IMXRT_LPSPI_t & port() FL_NOEXCEPT __attribute__((always_inline)) {
		switch(_SPI_INDEX) {
			case 0:
			return IMXRT_LPSPI4_S;
			case 1:
			return IMXRT_LPSPI3_S;
			case 2:
			return IMXRT_LPSPI1_S;
		}
	}

public:
	Teensy4HardwareSPIOutput() { mPSelect = nullptr; mBitCount = 0;}
	Teensy4HardwareSPIOutput(Selectable *pSelect) { mPSelect = pSelect; mBitCount = 0;}

	// set the object representing the selectable -- ignore for now
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() { _SPIObject.begin(); }

	// latch the CS select
	void inline select() FL_NOEXCEPT __attribute__((always_inline)) {
		// begin the SPI transaction
		_SPIObject.beginTransaction(SPISettings(_SPI_CLOCK_RATE, MSBFIRST, SPI_MODE0));
		if(mPSelect != nullptr) { mPSelect->select(); }
	}

	// release the CS select
	void inline release() FL_NOEXCEPT __attribute__((always_inline)) {
		if(mPSelect != nullptr) { mPSelect->release(); }
		_SPIObject.endTransaction();
	}

	void endTransaction() FL_NOEXCEPT {
		waitFully();
		release();
	}

	// wait until all queued up data has been written
	static void waitFully() { /* TODO */ }

	// write a byte out via SPI (returns immediately on writing register) -
	void inline writeByte(u8 b) FL_NOEXCEPT __attribute__((always_inline)) {
		if(mBitCount == 0) {
			_SPIObject.transfer(b);
		} else {
			// There's been a bit of data written, add that to the output as well
			u32 outData = (mBitData << 8) | b;
			u32 tcr = port().TCR;
			port().TCR = (tcr & 0xfffff000) | LPSPI_TCR_FRAMESZ((8+mBitCount) - 1);  // turn on 9 bit mode
			port().TDR = outData;		// output 9 bit data.
			while ((port().RSR & LPSPI_RSR_RXEMPTY)) ;	// wait while the RSR fifo is empty...
			port().TCR = (tcr & 0xfffff000) | LPSPI_TCR_FRAMESZ((8) - 1);  // turn back on 8 bit mode
			port().RDR;
			mBitCount = 0;
		}
	}

	// write a word out via SPI (returns immediately on writing register)
	void inline writeWord(u16 w) FL_NOEXCEPT __attribute__((always_inline)) {
		writeByte(((w>>8) & 0xFF));
		_SPIObject.transfer(w & 0xFF);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(u8 value, int len) FL_NOEXCEPT {
		while(len--) { _SPIObject.transfer(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(u8 value, int len) FL_NOEXCEPT {
		select(); writeBytesValueRaw(value, len); release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
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

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytes(FASTLED_REGISTER u8 *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <u8 BIT> inline void writeBit(u8 b) FL_NOEXCEPT {
		mBitData = (mBitData<<1) | ((b&(1<<BIT)) != 0);
		// If this is the 8th bit we've collected, just write it out raw
		FASTLED_REGISTER u32 bc = mBitCount;
		bc = (bc + 1) & 0x07;
		if (!bc) {
			mBitCount = 0;
			_SPIObject.transfer(mBitData);
		}
		mBitCount = bc;
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NOEXCEPT {
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

	/// Finalize transmission (no-op for Teensy 4.x SPI)
	/// This method exists for compatibility with other SPI implementations
	/// that may need to flush buffers or perform post-transmission operations
	static void finalizeTransmission() { }

};


#endif
}  // namespace fl

FL_DISABLE_WARNING_POP

#endif
