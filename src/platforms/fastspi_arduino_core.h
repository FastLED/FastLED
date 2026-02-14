#pragma once

// IWYU pragma: public

#include "fl/stl/stdint.h"

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

#if defined(ARDUNIO_CORE_SPI)
#endif

#if defined(ARDUNIO_CORE_SPI)
namespace fl {
template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_RATE, SPIClass & _SPIObject>
class ArdunioCoreSPIOutput {

public:
	ArdunioCoreSPIOutput() {}

	// set the object representing the selectable -- ignore for now
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() { _SPIObject.begin(); }

	// latch the CS select
	void inline select() __attribute__((always_inline)) {
		// begin the SPI transaction
		_SPIObject.beginTransaction(SPISettings(_SPI_CLOCK_RATE, MSBFIRST, SPI_MODE0));
	}

	// release the CS select
	void inline release() __attribute__((always_inline)) {
		_SPIObject.endTransaction();
	}

	void endTransaction() {
		waitFully();
		release();
	}

	// wait until all queued up data has been written
	static void waitFully() { /* TODO */ }

	// write a byte out via SPI (returns immediately on writing register) -
	void inline writeByte(u8 b) __attribute__((always_inline)) {
		_SPIObject.transfer(b);
	}

	// write a word out via SPI (returns immediately on writing register)
	void inline writeWord(u16 w) __attribute__((always_inline)) {
		_SPIObject.transfer16(w);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(u8 value, int len) {
		while(len--) { _SPIObject.transfer(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(u8 value, int len) {
		select(); writeBytesValueRaw(value, len); release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	template <class D> void writeBytes(FASTLED_REGISTER u8 *data, int len) {
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
	template <u8 BIT> inline void writeBit(u8 b) {
		// todo
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) {
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

	/// Finalize transmission (no-op for Arduino Core SPI)
	/// This method exists for compatibility with other SPI implementations
	/// that may need to flush buffers or perform post-transmission operations
	static void finalizeTransmission() { }

};
}  // namespace fl
#endif

FL_DISABLE_WARNING_POP
