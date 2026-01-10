#pragma once

#include "pixeltypes.h"
#include "fl/force_inline.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"
#include "platforms/shared/spi_pixel_writer.h"  // ok platform headers
#include "platforms/spi_output_template.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// LPD8806 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(12)
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint32_t SPI_SPEED = DATA_RATE_MHZ(12) >
class LPD8806Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;

	class LPD8806_ADJUST {
	public:
		// LPD8806 spec wants the high bit of every rgb data byte sent out to be set.
		FASTLED_FORCE_INLINE static fl::u8 adjust(FASTLED_REGISTER fl::u8 data) { return ((data>>1) | 0x80) + ((data && (data<254)) & 0x01); }
		FASTLED_FORCE_INLINE static void postBlock(int len, void* context = nullptr) {
			SPI* pSPI = static_cast<SPI*>(context);
			pSPI->writeBytesValueRaw(0, ((len*3+63)>>6));
		}

	};

	SPI mSPI;

public:
	LPD8806Controller()  {}
	virtual void init() {
		mSPI.init();
	}

protected:

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		fl::writePixelsToSPI<0, LPD8806_ADJUST, RGB_ORDER>(pixels, mSPI, &mSPI);
	}

public:
	/// Get the protocol-safe padding byte for LPD8806
	/// Used for quad-SPI lane padding when strips have different lengths
	/// @returns 0x00 (latch continuation byte)
	static constexpr fl::u8 getPaddingByte() { return 0x00; }

	/// Get a black LED frame for synchronized latching
	/// Used for quad-SPI lane padding to ensure all strips latch simultaneously
	/// @returns Black LED frame (invisible LED: GRB with MSB set)
	static fl::span<const fl::u8> getPaddingLEDFrame() {  // okay static in header
		static const fl::u8 frame[] = {  // okay static in header
			0x80,  // Green = 0 (with MSB=1)
			0x80,  // Red = 0 (with MSB=1)
			0x80   // Blue = 0 (with MSB=1)
		};
		return fl::span<const fl::u8>(frame, 3);
	}

	/// Get the size of the padding LED frame in bytes
	/// @returns 3 bytes per LED for LPD8806
	static constexpr size_t getPaddingLEDFrameSize() {
		return 3;
	}

	/// Calculate total byte count for LPD8806 protocol
	/// Used for quad-SPI buffer pre-allocation
	/// @param num_leds Number of LEDs in the strip
	/// @returns Total bytes needed (RGB data + latch bytes)
	static constexpr size_t calculateBytes(size_t num_leds) {
		// LPD8806 protocol:
		// - LED data: 3 bytes per LED (GRB with high bit set)
		// - Latch: ((num_leds * 3 + 63) / 64) bytes of 0x00
		return (num_leds * 3) + ((num_leds * 3 + 63) / 64);
	}
};

/// LPD6803 controller class (LPD1101).
/// 16 bit (1 bit const "1", 5 bit red, 5 bit green, 5 bit blue).
/// In chip CMODE pin must be set to 1 (inside oscillator mode).
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(12)
/// @see Datasheet: https://cdn-shop.adafruit.com/datasheets/LPD6803.pdf
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(12)>
class LPD6803Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() { mSPI.writeByte(0); mSPI.writeByte(0); mSPI.writeByte(0); mSPI.writeByte(0); }
	void endBoundary(int nLeds) { int nDWords = (nLeds/32); do { mSPI.writeByte(0xFF); mSPI.writeByte(0x00); mSPI.writeByte(0x00); mSPI.writeByte(0x00); } while(nDWords--); }

public:
	LPD6803Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		startBoundary();
		while(pixels.has(1)) {
            FASTLED_REGISTER fl::u16 command;
            command = 0x8000;
            command |= (pixels.loadAndScale0() & 0xF8) << 7; // red is the high 5 bits
            command |= (pixels.loadAndScale1() & 0xF8) << 2; // green is the middle 5 bits
			mSPI.writeByte((command >> 8) & 0xFF);
            command |= pixels.loadAndScale2() >> 3 ; // blue is the low 5 bits
			mSPI.writeByte(command & 0xFF);

			pixels.stepDithering();
			pixels.advanceData();
		}
		endBoundary(pixels.size());
		mSPI.endTransaction();
	}

};
