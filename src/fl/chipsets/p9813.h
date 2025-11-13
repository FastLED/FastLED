#pragma once

#include "pixeltypes.h"
#include "fl/force_inline.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"
#include "platforms/spi_output_template.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// P9813 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// P9813 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(10)
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(10)>
class P9813Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void writeBoundary() { mSPI.writeWord(0); mSPI.writeWord(0); }

	FASTLED_FORCE_INLINE void writeLed(fl::u8 r, fl::u8 g, fl::u8 b) {
		FASTLED_REGISTER fl::u8 top = 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
		mSPI.writeByte(top); mSPI.writeByte(b); mSPI.writeByte(g); mSPI.writeByte(r);
	}

public:
	P9813Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		writeBoundary();
		while(pixels.has(1)) {
			writeLed(pixels.loadAndScale0(), pixels.loadAndScale1(), pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
		writeBoundary();

		mSPI.endTransaction();
	}

public:
	/// Get the protocol-safe padding byte for P9813
	/// Used for quad-SPI lane padding when strips have different lengths
	/// @returns 0x00 (boundary byte)
	static constexpr fl::u8 getPaddingByte() { return 0x00; }

	/// Get a black LED frame for synchronized latching
	/// Used for quad-SPI lane padding to ensure all strips latch simultaneously
	/// @returns Black LED frame (invisible LED: flag byte + BGR all zero)
	static fl::span<const fl::u8> getPaddingLEDFrame() {  // okay static in header
		static const fl::u8 frame[] = {  // okay static in header
			0xFF,  // Flag byte for RGB=0,0,0
			0x00,  // Blue = 0
			0x00,  // Green = 0
			0x00   // Red = 0
		};
		return fl::span<const fl::u8>(frame, 4);
	}

	/// Get the size of the padding LED frame in bytes
	/// @returns 4 bytes per LED for P9813
	static constexpr size_t getPaddingLEDFrameSize() {
		return 4;
	}

	/// Calculate total byte count for P9813 protocol
	/// Used for quad-SPI buffer pre-allocation
	/// @param num_leds Number of LEDs in the strip
	/// @returns Total bytes needed (boundaries + LED data)
	static constexpr size_t calculateBytes(size_t num_leds) {
		// P9813 protocol:
		// - Start boundary: 4 bytes (0x00000000)
		// - LED data: 4 bytes per LED (flag byte + BGR)
		// - End boundary: 4 bytes (0x00000000)
		return 4 + (num_leds * 4) + 4;
	}
};
