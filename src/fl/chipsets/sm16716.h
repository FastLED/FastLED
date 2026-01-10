#pragma once

#include "pixeltypes.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"
#include "platforms/shared/spi_pixel_writer.h"  // ok platform headers
#include "platforms/spi_output_template.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SM16716 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// SM16716 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(16)
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(16)>
class SM16716Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void writeHeader() {
		// Write out 50 zeros to the spi line (6 blocks of 8 followed by two single bit writes)
		mSPI.select();
		mSPI.template writeBit<0>(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.template writeBit<0>(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		// Note: endTransaction() may not be strictly necessary for SM16716
		// since we're just streaming bytes. However, it's kept here for consistency
		// with other SPI-based controllers and as defensive programming.
		mSPI.endTransaction();
	}

public:
	SM16716Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		// writeHeader();
		fl::writePixelsToSPI<FLAG_START_BIT, DATA_NOP, RGB_ORDER>(pixels, mSPI, nullptr);
		writeHeader();
	}

};
