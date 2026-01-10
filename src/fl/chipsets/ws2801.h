#pragma once

#include "pixeltypes.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"
#include "platforms/shared/spi_pixel_writer.h"  // ok platform headers
#include "platforms/spi_output_template.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// WS2801 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(1)
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(1)>
class WS2801Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	CMinWait<1000>  mWaitDelay;

public:
	WS2801Controller() {}

	/// Initialize the controller
	virtual void init() {
		mSPI.init();
	  mWaitDelay.mark();
	}

protected:

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWaitDelay.wait();
		fl::writePixelsToSPI<0, DATA_NOP, RGB_ORDER>(pixels, mSPI, nullptr);
		mWaitDelay.mark();
	}

public:
	/// Get the protocol-safe padding byte for WS2801
	/// Used for quad-SPI lane padding when strips have different lengths
	/// @returns 0x00 (no protocol state)
	static constexpr fl::u8 getPaddingByte() { return 0x00; }

	/// Get a black LED frame for synchronized latching
	/// Used for quad-SPI lane padding to ensure all strips latch simultaneously
	/// @returns Black LED frame (invisible LED: RGB all zero)
	static fl::span<const fl::u8> getPaddingLEDFrame() {  // okay static in header
		static const fl::u8 frame[] = {  // okay static in header
			0x00,  // Red = 0
			0x00,  // Green = 0
			0x00   // Blue = 0
		};
		return fl::span<const fl::u8>(frame, 3);
	}

	/// Get the size of the padding LED frame in bytes
	/// @returns 3 bytes per LED for WS2801
	static constexpr size_t getPaddingLEDFrameSize() {
		return 3;
	}

	/// Calculate total byte count for WS2801 protocol
	/// Used for quad-SPI buffer pre-allocation
	/// @param num_leds Number of LEDs in the strip
	/// @returns Total bytes needed (RGB data only, no overhead)
	static constexpr size_t calculateBytes(size_t num_leds) {
		// WS2801 protocol:
		// - LED data: 3 bytes per LED (RGB)
		// - No frame overhead (latch is timing-based, not data-based)
		return num_leds * 3;
	}
};

/// WS2803 controller class.
/// @copydetails WS2801Controller
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(25)>
class WS2803Controller : public WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};
