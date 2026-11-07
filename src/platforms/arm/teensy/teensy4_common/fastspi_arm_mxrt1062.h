// IWYU pragma: private

#ifndef __INC_FASTSPI_ARM_MXRT1062_H
#define __INC_FASTSPI_ARM_MXRT1062_H

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)
// Local LPSPI fork instead of the Arduino SPI library -- see
// platforms/arm/teensy/teensy4_common/lpspi/lpspi_hardware.h (#3777).
#include "platforms/arm/teensy/teensy4_common/lpspi/lpspi_bus.h"
#include "fastspi_types.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/system/delay.h"
#include "fl/system/fastpin_base.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_RATE, int _SPI_INDEX>
class Teensy4HardwareSPIOutput {
	FL_STATIC_ASSERT(_SPI_INDEX >= 0 && _SPI_INDEX <= 2,
		"Teensy 4 has LPSPI indices 0 (LPSPI4), 1 (LPSPI3) and 2 (LPSPI1) only");

	Selectable *mPSelect = nullptr;
	u32  mBitCount = 0;   // Bits collected by writeBit and not yet sent (0-7).
	u32 mBitData = 0;     // Those bits, oldest in the highest position.

	// Send `bits` (8-16) low bits of `data`, MSB first, as one LPSPI frame and
	// wait until it has been clocked out. LPSPI frames are at least 8 bits.
	void sendFrame(u32 data, u32 bits) FL_NO_EXCEPT {
		const u32 tcr = port().TCR;
		port().TCR = (tcr & 0xfffff000) | LPSPI_TCR_FRAMESZ(bits - 1);
		port().TDR = data;
		while ((port().RSR & LPSPI_RSR_RXEMPTY)) ;	// The frame is done once its receive word lands.
		// Force 8-bit frames back: TCR reads can lag the command FIFO, so a read
		// value is not a safe thing to restore.
		port().TCR = (tcr & 0xfffff000) | LPSPI_TCR_FRAMESZ(7);
		const u32 received = port().RDR;  // Pop the word so the RX FIFO stays empty.
		(void)received;
	}

	// Send 1-7 bits, which no LPSPI frame can carry: hand both pads to GPIO,
	// clock the bits out in mode 0 at the SPI rate, and give the pads back. Every
	// LPSPI write here is blocking, so the shifter is already idle.
	void sendBitsByGpio(u32 data, u32 bits) FL_NO_EXCEPT {
		volatile u32 *dataMux = portConfigRegister(_DATA_PIN);
		volatile u32 *clockMux = portConfigRegister(_CLOCK_PIN);
		volatile u32 *dataPad = portControlRegister(_DATA_PIN);
		volatile u32 *clockPad = portControlRegister(_CLOCK_PIN);
		const u32 savedDataMux = *dataMux;
		const u32 savedClockMux = *clockMux;
		const u32 savedDataPad = *dataPad;
		const u32 savedClockPad = *clockPad;
		const u32 halfPeriodNs = 500000000UL / _SPI_CLOCK_RATE;
		FastPin<_CLOCK_PIN>::lo();  // Mode 0 idles low; set the latch before the mux moves.
		FastPin<_CLOCK_PIN>::setOutput();
		FastPin<_DATA_PIN>::setOutput();
		for (u32 i = bits; i-- > 0;) {
			if ((data >> i) & 1U) {
				FastPin<_DATA_PIN>::hi();
			} else {
				FastPin<_DATA_PIN>::lo();
			}
			fl::delayNanoseconds(halfPeriodNs);
			FastPin<_CLOCK_PIN>::hi();
			fl::delayNanoseconds(halfPeriodNs);
			FastPin<_CLOCK_PIN>::lo();
		}
		*dataMux = savedDataMux;
		*clockMux = savedClockMux;
		*dataPad = savedDataPad;
		*clockPad = savedClockPad;
	}

	// Send any bits writeBit has collected, exactly that many.
	void flushBits() FL_NO_EXCEPT {
		if (mBitCount != 0) {
			sendBitsByGpio(mBitData, mBitCount);
			mBitCount = 0;
			mBitData = 0;
		}
	}

	inline IMXRT_LPSPI_t & port() FL_NO_EXCEPT __attribute__((always_inline)) {
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
	void init() { fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).begin(); }

	// latch the CS select
	void inline select() FL_NO_EXCEPT __attribute__((always_inline)) {
		// begin the SPI transaction
		fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).beginTransaction(
			fl::platforms::teensy::LpspiSettings(_SPI_CLOCK_RATE, MSBFIRST, SPI_MODE0));
		if(mPSelect != nullptr) { mPSelect->select(); }
	}

	// release the CS select, after any pending bits have gone out
	void inline release() FL_NO_EXCEPT __attribute__((always_inline)) {
		flushBits();
		if(mPSelect != nullptr) { mPSelect->release(); }
		fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).endTransaction();
	}

	void endTransaction() FL_NO_EXCEPT {
		waitFully();
		release();
	}

	// wait until all queued up data has been written. Every transfer here is
	// blocking, so only bits still held by writeBit can be outstanding.
	void waitFully() FL_NO_EXCEPT { flushBits(); }

	// write a byte out via SPI. Pending writeBit bits lead it in the same frame.
	void inline writeByte(u8 b) FL_NO_EXCEPT __attribute__((always_inline)) {
		if(mBitCount == 0) {
			fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).transfer(b);
		} else {
			sendFrame((mBitData << 8) | b, 8 + mBitCount);
			mBitCount = 0;
			mBitData = 0;
		}
	}

	// write a word out via SPI (blocking, like writeByte)
	void inline writeWord(u16 w) FL_NO_EXCEPT __attribute__((always_inline)) {
		writeByte(((w>>8) & 0xFF));
		fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).transfer(w & 0xFF);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere.
	// Pending writeBit bits go out first so the order on the wire is kept.
	void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
		flushBits();
		while(len--) { fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).transfer(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
		select(); writeBytesValueRaw(value, len); release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	template <class D> void writeBytes(FASTLED_REGISTER u8 *data, int len) FL_NO_EXCEPT {
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
	template <u8 BIT> inline void writeBit(u8 b) FL_NO_EXCEPT {
		mBitData = (mBitData<<1) | ((b&(1<<BIT)) != 0);
		// Eight collected bits are a whole byte: send it as one.
		if (++mBitCount == 8) {
			const u8 whole = static_cast<u8>(mBitData);
			mBitCount = 0;
			mBitData = 0;
			fl::platforms::teensy::LpspiBus::get(_SPI_INDEX).transfer(whole);
		}
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NO_EXCEPT {
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

	/// Finalize transmission: send any bits writeBit still holds.
	void finalizeTransmission() FL_NO_EXCEPT { flushBits(); }

};

}  // namespace fl

FL_DISABLE_WARNING_POP

#endif  // FL_IS_TEENSY_4X

#endif
