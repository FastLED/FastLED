/// @file dmx.h
/// Defines the DMX512-based LED controllers.

#ifndef __INC_DMX_H
#define __INC_DMX_H

#include "FastLED.h"

/// @addtogroup Chipsets
/// @{

/// @addtogroup ClocklessChipsets
/// @{

#if defined(DmxSimple_h) || defined(FASTLED_DOXYGEN)
#include <DmxSimple.h>

/// Flag set when the DmxSimple library is included
#define HAS_DMX_SIMPLE

FASTLED_NAMESPACE_BEGIN

/// DMX512 based LED controller class, using the DmxSimple library
/// @tparam DATA_PIN the data pin for the output of the DMX bus
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @see https://www.pjrc.com/teensy/td_libs_DmxSimple.html
/// @see https://github.com/PaulStoffregen/DmxSimple
/// @see https://en.wikipedia.org/wiki/DMX512
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB> class DMXSimpleController : public CPixelLEDController<RGB_ORDER> {
public:
	/// Initialize the LED controller
	virtual void init() { DmxSimple.usePin(DATA_PIN); }

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		int iChannel = 1;
		while(pixels.has(1)) {
			DmxSimple.write(iChannel++, pixels.loadAndScale0());
			DmxSimple.write(iChannel++, pixels.loadAndScale1());
			DmxSimple.write(iChannel++, pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
	}
};

FASTLED_NAMESPACE_END

#endif

#if defined(DmxSerial_h) || defined(FASTLED_DOXYGEN)
#include <DMXSerial.h>

/// Flag set when the DMXSerial library is included
#define HAS_DMX_SERIAL

FASTLED_NAMESPACE_BEGIN

/// DMX512 based LED controller class, using the DMXSerial library
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @see http://www.mathertel.de/Arduino/DMXSerial.aspx
/// @see https://github.com/mathertel/DMXSerial
/// @see https://en.wikipedia.org/wiki/DMX512
template <EOrder RGB_ORDER = RGB> class DMXSerialController : public CPixelLEDController<RGB_ORDER> {
public:
	/// Initialize the LED controller
	virtual void init() { DMXSerial.init(DMXController); }

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		int iChannel = 1;
		while(pixels.has(1)) {
			DMXSerial.write(iChannel++, pixels.loadAndScale0());
			DMXSerial.write(iChannel++, pixels.loadAndScale1());
			DMXSerial.write(iChannel++, pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
	}
};

FASTLED_NAMESPACE_END

/// @} DMXControllers
/// @} Chipsets

#endif

#endif
