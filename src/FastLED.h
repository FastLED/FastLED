#pragma once
#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include <stdint.h>

/// @file FastLED.h
/// central include file for FastLED, defines the CFastLED class/object

#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#define FASTLED_HAS_PRAGMA_MESSAGE
#endif

/// Current FastLED version number, as an integer.
/// E.g. 3007001 for version "3.7.1", with:
/// * 1 digit for the major version
/// * 3 digits for the minor version
/// * 3 digits for the patch version
#define FASTLED_VERSION 3009014
#ifndef FASTLED_INTERNAL
#  ifdef  FASTLED_SHOW_VERSION
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "FastLED version 3.009.014"
#    else
#      warning FastLED version 3.009.014  (Not really a warning, just telling you here.)
#    endif
#  endif
#endif

#ifndef __PROG_TYPES_COMPAT__
/// avr-libc define to expose __progmem__ typedefs.
/// @note These typedefs are now deprecated!
/// @see https://www.nongnu.org/avr-libc/user-manual/group__avr__pgmspace.html
#define __PROG_TYPES_COMPAT__
#endif

#ifdef SmartMatrix_h
#include <SmartMatrix.h>
#endif

#ifdef DmxSimple_h
#include <DmxSimple.h>
#endif

#ifdef DmxSerial_h
#include <DMXSerial.h>
#endif

#ifdef USE_OCTOWS2811
#include <OctoWS2811.h>
#endif



#include "fl/force_inline.h"
#include "cpp_compat.h"

#include "fastled_config.h"
#include "led_sysdefs.h"

// Utility functions
#include "fastled_delay.h"
#include "bitswap.h"

#include "controller.h"
#include "fastpin.h"
#include "fastspi_types.h"
#include "dmx.h"

#include "platforms.h"
#include "fastled_progmem.h"

#include "lib8tion.h"
#include "pixeltypes.h"
#include "hsv2rgb.h"
#include "colorutils.h"
#include "pixelset.h"
#include "colorpalettes.h"

#include "noise.h"
#include "power_mgt.h"

#include "fastspi.h"
#include "chipsets.h"
#include "fl/engine_events.h"

FASTLED_NAMESPACE_BEGIN

// Backdoor to get the size of the CLedController object. The one place
// that includes this just uses extern to declare the function.
uint16_t cled_contoller_size();

/// LED chipsets with SPI interface
enum ESPIChipsets {
	LPD6803,  ///< LPD6803 LED chipset
	LPD8806,  ///< LPD8806 LED chipset
	WS2801,   ///< WS2801 LED chipset
	WS2803,   ///< WS2803 LED chipset
	SM16716,  ///< SM16716 LED chipset
	P9813,    ///< P9813 LED chipset
	APA102,   ///< APA102 LED chipset
	SK9822,   ///< SK9822 LED chipset
	SK9822HD, ///< SK9822 LED chipset with 5-bit gamma correction
	DOTSTAR,  ///< APA102 LED chipset alias
	DOTSTARHD, ///< APA102HD LED chipset alias
	APA102HD, ///< APA102 LED chipset with 5-bit gamma correction
	HD107,  /// Same as APA102, but in turbo 40-mhz mode.
	HD107HD,  /// Same as APA102HD, but in turbo 40-mhz mode.

};

/// Smart Matrix Library controller type
/// @see https://github.com/pixelmatix/SmartMatrix
enum ESM { SMART_MATRIX };

/// Octo WS2811 LED Library controller types
/// @see https://www.pjrc.com/teensy/td_libs_OctoWS2811.html
/// @see https://github.com/PaulStoffregen/OctoWS2811
enum OWS2811 { OCTOWS2811,OCTOWS2811_400, OCTOWS2813};

/// WS2812Serial Library controller type
/// @see https://www.pjrc.com/non-blocking-ws2812-led-library/
/// @see https://github.com/PaulStoffregen/WS2812Serial
enum SWS2812 { WS2812SERIAL };

#ifdef HAS_PIXIE
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class PIXIE : public PixieController<DATA_PIN, RGB_ORDER> {};
#endif

#ifdef FASTLED_HAS_CLOCKLESS
/// @addtogroup Chipsets
/// @{
/// @addtogroup ClocklessChipsets
/// @{

/// LED controller for WS2812 LEDs with GRB color order
/// @see WS2812Controller800Khz
template<uint8_t DATA_PIN> class NEOPIXEL : public WS2812Controller800Khz<DATA_PIN, GRB> {};

/// @brief SM16703 controller class.
/// @copydetails SM16703Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER> 
class SM16703 : public SM16703Controller<DATA_PIN, RGB_ORDER> {};

/// @brief TM1829 controller class.
/// @copydetails TM1829Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class TM1829 : public TM1829Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1812 controller class.
/// @copydetails TM1809Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class TM1812 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1809 controller class.
/// @copydetails TM1809Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class TM1809 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1804 controller class.
/// @copydetails TM1809Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class TM1804 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1803 controller class.
/// @copydetails TM1803Controller400Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class TM1803 : public TM1803Controller400Khz<DATA_PIN, RGB_ORDER> {}; 

/// @brief UCS1903 controller class.
/// @copydetails UCS1903Controller400Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class UCS1903 : public UCS1903Controller400Khz<DATA_PIN, RGB_ORDER> {};

/// @brief UCS1903B controller class.
/// @copydetails UCS1903BController800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class UCS1903B : public UCS1903BController800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief UCS1904 controller class.
/// @copydetails UCS1904Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class UCS1904 : public UCS1904Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief UCS2903 controller class.
/// @copydetails UCS2903Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class UCS2903 : public UCS2903Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2812 controller class.
/// @copydetails WS2812Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2812 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2815 controller class.
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2815 : public WS2815Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2816 controller class.
template <uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2816 : public WS2816Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2852 controller class.
/// @copydetails WS2812Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2852 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2812B controller class.
/// @copydetails WS2812Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2812B : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GS1903 controller class.
/// @copydetails WS2812Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class GS1903 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief SK6812 controller class.
/// @copydetails SK6812Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class SK6812 : public SK6812Controller<DATA_PIN, RGB_ORDER> {};

/// @brief SK6822 controller class.
/// @copydetails SK6822Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class SK6822 : public SK6822Controller<DATA_PIN, RGB_ORDER> {};

/// @brief APA106 controller class.
/// @copydetails SK6822Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class APA106 : public SK6822Controller<DATA_PIN, RGB_ORDER> {};

/// @brief PL9823 controller class.
/// @copydetails PL9823Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class PL9823 : public PL9823Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2811 controller class.
/// @copydetails WS2811Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2811 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2813 controller class.
/// @copydetails WS2813Controller
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2813 : public WS2813Controller<DATA_PIN, RGB_ORDER> {};

/// @brief APA104 controller class.
/// @copydetails WS2811Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class APA104 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2811_400 controller class.
/// @copydetails WS2811Controller400Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class WS2811_400 : public WS2811Controller400Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GE8822 controller class.
/// @copydetails GE8822Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class GE8822 : public GE8822Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GW6205 controller class.
/// @copydetails GW6205Controller800Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class GW6205 : public GW6205Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GW6205_400 controller class.
/// @copydetails GW6205Controller400Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class GW6205_400 : public GW6205Controller400Khz<DATA_PIN, RGB_ORDER> {};

/// @brief LPD1886 controller class.
/// @copydetails LPD1886Controller1250Khz
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class LPD1886 : public LPD1886Controller1250Khz<DATA_PIN, RGB_ORDER> {};

/// @brief LPD1886_8BIT controller class.
/// @copydetails LPD1886Controller1250Khz_8bit
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class LPD1886_8BIT : public LPD1886Controller1250Khz_8bit<DATA_PIN, RGB_ORDER> {};

/// @brief UCS1912 controller class.
template<uint8_t DATA_PIN, EOrder RGB_ORDER>
class UCS1912 : public UCS1912Controller<DATA_PIN, RGB_ORDER> {};

#if defined(DmxSimple_h) || defined(FASTLED_DOXYGEN)
/// @copydoc DMXSimpleController
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class DMXSIMPLE : public DMXSimpleController<DATA_PIN, RGB_ORDER> {};
#endif
#if defined(DmxSerial_h) || defined(FASTLED_DOXYGEN)
/// @copydoc DMXSerialController
template<EOrder RGB_ORDER> class DMXSERIAL : public DMXSerialController<RGB_ORDER> {};
#endif
#endif
/// @} ClocklessChipsets
/// @} Chipsets


/// Blockless output port enum
enum EBlockChipsets {
#ifdef PORTA_FIRST_PIN
	WS2811_PORTA,
	WS2813_PORTA,
	WS2811_400_PORTA,
	TM1803_PORTA,
	UCS1903_PORTA,
#endif
#ifdef PORTB_FIRST_PIN
	WS2811_PORTB,
	WS2813_PORTB,
	WS2811_400_PORTB,
	TM1803_PORTB,
	UCS1903_PORTB,
#endif
#ifdef PORTC_FIRST_PIN
	WS2811_PORTC,
	WS2813_PORTC,
	WS2811_400_PORTC,
	TM1803_PORTC,
	UCS1903_PORTC,
#endif
#ifdef PORTD_FIRST_PIN
	WS2811_PORTD,
	WS2813_PORTD,
	WS2811_400_PORTD,
	TM1803_PORTD,
	UCS1903_PORTD,
#endif
#ifdef HAS_PORTDC
	WS2811_PORTDC,
	WS2813_PORTDC,
	WS2811_400_PORTDC,
	TM1803_PORTDC,
	UCS1903_PORTDC,
#endif
};

/// Typedef for a power consumption calculation function. Used within
/// CFastLED for rescaling brightness before sending the LED data to
/// the strip with CFastLED::show().
/// @param scale the initial brightness scale value
/// @param data max power data, in milliwatts
/// @returns the brightness scale, limited to max power
typedef uint8_t (*power_func)(uint8_t scale, uint32_t data);

/// High level controller interface for FastLED.
/// This class manages controllers, global settings, and trackings such as brightness
/// and refresh rates, and provides access functions for driving led data to controllers
/// via the show() / showColor() / clear() methods.
/// This is instantiated as a global object with the name FastLED.
/// @nosubgrouping
class CFastLED {
	// int m_nControllers;
	uint8_t  m_Scale;         ///< the current global brightness scale setting
	uint16_t m_nFPS;          ///< tracking for current frames per second (FPS) value
	uint32_t m_nMinMicros;    ///< minimum µs between frames, used for capping frame rates
	uint32_t m_nPowerData;    ///< max power use parameter
	power_func m_pPowerFunc;  ///< function for overriding brightness when using FastLED.show();

public:
	CFastLED();

	// Useful when you want to know when an event like onFrameBegin or onFrameEnd is happening.
	// This is disabled on AVR to save space.
	void addListener(fl::EngineEvents::Listener *listener) { fl::EngineEvents::addListener(listener); }
	void removeListener(fl::EngineEvents::Listener *listener) { fl::EngineEvents::removeListener(listener); }

	/// Add a CLEDController instance to the world.  Exposed to the public to allow people to implement their own
	/// CLEDController objects or instances.  There are two ways to call this method (as well as the other addLeds()
	/// variations). The first is with 3 arguments, in which case the arguments are the controller, a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 4 arguments, in which case
	/// the first two arguments are the same, the third argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the fourth argument is the number of leds for this controller object.
	/// @param pLed the led controller being added
	/// @param data base pointer to an array of CRGB data structures
	/// @param nLedsOrOffset number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset number of leds (4 argument version)
	/// @returns a reference to the added controller
	static CLEDController &addLeds(CLEDController *pLed, struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

	/// @name Adding SPI-based controllers
	/// Add an SPI based CLEDController instance to the world.
	///
	/// There are two ways to call this method (as well as the other addLeds() 
	/// variations).  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object.
	///
	/// @param data base pointer to an array of CRGB data structures
	/// @param nLedsOrOffset number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset number of leds (4 argument version)
	/// @tparam CHIPSET the chipset type
	/// @tparam DATA_PIN the optional data pin for the leds (if omitted, will default to the first hardware SPI MOSI pin)
	/// @tparam CLOCK_PIN the optional clock pin for the leds (if omitted, will default to the first hardware SPI clock pin)
	/// @tparam RGB_ORDER the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @tparam SPI_DATA_RATE the data rate to drive the SPI clock at, defined using DATA_RATE_MHZ or DATA_RATE_KHZ macros
	/// @returns a reference to the added controller
	/// @{


	// Base template: Causes a compile-time error if an unsupported CHIPSET is used
	template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN>
	struct ClockedChipsetHelper {
	    // Default implementation, will be specialized for supported chipsets
		static const bool IS_VALID = false;
	};

	// Macro to define a mapping from the ESPIChipeset enum to the controller class
	// in it's various template configurations.
	#define _FL_MAP_CLOCKED_CHIPSET(CHIPSET_ENUM, CONTROLLER_CLASS)                              \
		template<uint8_t DATA_PIN, uint8_t CLOCK_PIN>                                              \
		struct ClockedChipsetHelper<CHIPSET_ENUM, DATA_PIN, CLOCK_PIN> {                           \
		    static const bool IS_VALID = true;                                                     \
			typedef CONTROLLER_CLASS<DATA_PIN, CLOCK_PIN> ControllerType;                          \
			/* Controller type with RGB_ORDER specified */                                         \
			template<EOrder RGB_ORDER>															   \
			struct CONTROLLER_CLASS_WITH_ORDER {                                                   \
				typedef CONTROLLER_CLASS<DATA_PIN, CLOCK_PIN, RGB_ORDER> ControllerType;           \
			};                                                                                     \
			/* Controller type with RGB_ORDER and spi frequency specified */                       \
			template<EOrder RGB_ORDER, uint32_t FREQ>                                              \
			struct CONTROLLER_CLASS_WITH_ORDER_AND_FREQ {                                          \
				typedef CONTROLLER_CLASS<DATA_PIN, CLOCK_PIN, RGB_ORDER, FREQ> ControllerType;     \
			};                                                                                     \
		};

	// Define specializations for each supported CHIPSET
	_FL_MAP_CLOCKED_CHIPSET(LPD6803, LPD6803Controller)
	_FL_MAP_CLOCKED_CHIPSET(LPD8806, LPD8806Controller)
	_FL_MAP_CLOCKED_CHIPSET(WS2801, WS2801Controller)
	_FL_MAP_CLOCKED_CHIPSET(WS2803, WS2803Controller)
	_FL_MAP_CLOCKED_CHIPSET(SM16716, SM16716Controller)
	_FL_MAP_CLOCKED_CHIPSET(P9813, P9813Controller)

	// Both DOTSTAR and APA102 use the same controller class
	_FL_MAP_CLOCKED_CHIPSET(DOTSTAR, APA102Controller)
	_FL_MAP_CLOCKED_CHIPSET(APA102, APA102Controller)

	// Both DOTSTARHD and APA102HD use the same controller class
	_FL_MAP_CLOCKED_CHIPSET(DOTSTARHD, APA102ControllerHD)
	_FL_MAP_CLOCKED_CHIPSET(APA102HD, APA102ControllerHD)

	_FL_MAP_CLOCKED_CHIPSET(HD107, APA102Controller)
	_FL_MAP_CLOCKED_CHIPSET(HD107HD, APA102ControllerHD)

	_FL_MAP_CLOCKED_CHIPSET(SK9822, SK9822Controller)
	_FL_MAP_CLOCKED_CHIPSET(SK9822HD, SK9822ControllerHD)


	/// Add an SPI based CLEDController instance to the world.
	template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE > CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		// Instantiate the controller using ClockedChipsetHelper
		typedef ClockedChipsetHelper<CHIPSET, DATA_PIN, CLOCK_PIN> CHIP;
		typedef typename CHIP::template CONTROLLER_CLASS_WITH_ORDER_AND_FREQ<RGB_ORDER, SPI_DATA_RATE>::ControllerType ControllerTypeWithFreq;
		static_assert(CHIP::IS_VALID, "Unsupported chipset");
		static ControllerTypeWithFreq c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add an SPI based CLEDController instance to the world.
	template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN > static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		typedef ClockedChipsetHelper<CHIPSET, DATA_PIN, CLOCK_PIN> CHIP;
		typedef typename CHIP::ControllerType ControllerType;
		static_assert(CHIP::IS_VALID, "Unsupported chipset");
		static ControllerType c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}


	// The addLeds function using ChipsetHelper
	template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER>
	CLEDController& addLeds(struct CRGB* data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		typedef ClockedChipsetHelper<CHIPSET, DATA_PIN, CLOCK_PIN> CHIP;
		static_assert(CHIP::IS_VALID, "Unsupported chipset");
		typedef typename CHIP::template CONTROLLER_CLASS_WITH_ORDER<RGB_ORDER>::ControllerType ControllerTypeWithOrder;
		static ControllerTypeWithOrder c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}


#ifdef SPI_DATA
	template<ESPIChipsets CHIPSET> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER, SPI_DATA_RATE>(data, nLedsOrOffset, nLedsIfOffset);
	}

#endif
	/// @} Adding SPI based controllers

#ifdef FASTLED_HAS_CLOCKLESS
	/// @name Adding 3-wire led controllers
	/// Add a clockless (aka 3-wire, also DMX) based CLEDController instance to the world.
	///
	/// There are two ways to call this method (as well as the other addLeds()
	/// variations). The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object.
	///
	/// This method also takes 2 to 3 template parameters for identifying the specific chipset, data pin,
	/// RGB ordering, and SPI data rate
	///
	/// @param data base pointer to an array of CRGB data structures
	/// @param nLedsOrOffset number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset number of leds (4 argument version)
	/// @tparam CHIPSET the chipset type (required)
	/// @tparam DATA_PIN the data pin for the leds (required)
	/// @tparam RGB_ORDER the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @returns a reference to the added controller
	/// @{

	/// Add a clockless based CLEDController instance to the world.
	template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add a clockless based CLEDController instance to the world.
	template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add a clockless based CLEDController instance to the world.
	template<template<uint8_t DATA_PIN> class CHIPSET, uint8_t DATA_PIN>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

#if defined(__FASTLED_HAS_FIBCC) && (__FASTLED_HAS_FIBCC == 1)
	template<uint8_t NUM_LANES, template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER=RGB>
	static CLEDController &addLeds(struct CRGB *data, int nLeds) {
		static __FIBCC<CHIPSET, DATA_PIN, NUM_LANES, RGB_ORDER> c;
		return addLeds(&c, data, nLeds);
	}
#endif

	#ifdef FASTSPI_USE_DMX_SIMPLE
	template<EClocklessChipsets CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER=RGB>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case DMX: { static DMXController<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
	#endif
	/// @} Adding 3-wire led controllers
#endif

	/// @name Adding 3rd party library controllers
	/// Add a 3rd party library based CLEDController instance to the world.
	///
	/// There are two ways to call this method (as well as the other addLeds()
	/// variations).  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object. This class includes the SmartMatrix
	/// and OctoWS2811 based controllers
	///
	/// This method also takes 1 to 2 template parameters for identifying the specific chipset and
	/// RGB ordering.
	///
	/// @param data base pointer to an array of CRGB data structures
	/// @param nLedsOrOffset number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset number of leds (4 argument version)
	/// @tparam CHIPSET the chipset type (required)
	/// @tparam RGB_ORDER the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @returns a reference to the added controller
	/// @{

	/// Add a 3rd party library based CLEDController instance to the world.
	template<template<EOrder RGB_ORDER> class CHIPSET, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add a 3rd party library based CLEDController instance to the world.
	template<template<EOrder RGB_ORDER> class CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

#ifdef USE_OCTOWS2811
	/// Add a OCTOWS2811 based CLEDController instance to the world.
	/// @see https://www.pjrc.com/teensy/td_libs_OctoWS2811.html
	/// @see https://github.com/PaulStoffregen/OctoWS2811
	template<OWS2811 CHIPSET, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case OCTOWS2811: { static COctoWS2811Controller<RGB_ORDER,WS2811_800kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case OCTOWS2811_400: { static COctoWS2811Controller<RGB_ORDER,WS2811_400kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#ifdef WS2813_800kHz
      case OCTOWS2813: { static COctoWS2811Controller<RGB_ORDER,WS2813_800kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#endif
		}
	}

	/// Add a OCTOWS2811 library based CLEDController instance to the world.
	/// @see https://www.pjrc.com/teensy/td_libs_OctoWS2811.html
	/// @see https://github.com/PaulStoffregen/OctoWS2811
	template<OWS2811 CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		return addLeds<CHIPSET,GRB>(data,nLedsOrOffset,nLedsIfOffset);
	}

#endif

#ifdef USE_WS2812SERIAL
	/// Add a WS2812Serial library based CLEDController instance to the world.
	/// @see https://www.pjrc.com/non-blocking-ws2812-led-library/
	/// @see https://github.com/PaulStoffregen/WS2812Serial
	template<SWS2812 CHIPSET, int DATA_PIN, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		static CWS2812SerialController<DATA_PIN,RGB_ORDER> controller;
		return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset);
	}
#endif

#ifdef SmartMatrix_h
	/// Add a SmartMatrix library based CLEDController instance to the world.
	/// @see https://github.com/pixelmatix/SmartMatrix
	template<ESM CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case SMART_MATRIX: { static CSmartMatrixController controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
#endif
	/// @} Adding 3rd party library controllers


#ifdef FASTLED_HAS_BLOCKLESS

	/// @name Adding parallel output controllers
	/// Add a block based CLEDController instance to the world.
	///
	/// There are two ways to call this method (as well as the other addLeds()
	/// variations).  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object.
	///
	/// This method also takes a 2 to 3 template parameters for identifying the specific chipset and rgb ordering
	/// RGB ordering, and SPI data rate
	///
	/// @param data base pointer to an array of CRGB data structures
	/// @param nLedsOrOffset number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset number of leds (4 argument version)
	/// @tparam CHIPSET the chipset/port type (required)
	/// @tparam NUM_LANES how many parallel lanes of output to write
	/// @tparam RGB_ORDER the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @returns a reference to the added controller
	/// @{

	/// Add a block based parallel output CLEDController instance to the world.
	template<EBlockChipsets CHIPSET, int NUM_LANES, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
		#ifdef PORTA_FIRST_PIN
				case WS2811_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTB_FIRST_PIN
				case WS2811_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTC_FIRST_PIN
				case WS2811_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTD_FIRST_PIN
				case WS2811_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef HAS_PORTDC
				case WS2811_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES,NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES,NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		}
	}

	/// Add a block based parallel output CLEDController instance to the world.
	template<EBlockChipsets CHIPSET, int NUM_LANES>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET,NUM_LANES,GRB>(data,nLedsOrOffset,nLedsIfOffset);
	}
	/// @} Adding parallel output controllers
#endif

	/// Set the global brightness scaling
	/// @param scale a 0-255 value for how much to scale all leds before writing them out
	void setBrightness(uint8_t scale) { m_Scale = scale; }

	/// Get the current global brightness setting
	/// @returns the current global brightness value
	uint8_t getBrightness() { return m_Scale; }

	/// Set the maximum power to be used, given in volts and milliamps.
	/// @param volts how many volts the leds are being driven at (usually 5)
	/// @param milliamps the maximum milliamps of power draw you want
	inline void setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps) { setMaxPowerInMilliWatts(volts * milliamps); }

	/// Set the maximum power to be used, given in milliwatts
	/// @param milliwatts the max power draw desired, in milliwatts
	inline void setMaxPowerInMilliWatts(uint32_t milliwatts) { m_pPowerFunc = &calculate_max_brightness_for_power_mW; m_nPowerData = milliwatts; }

	/// Update all our controllers with the current led colors, using the passed in brightness
	/// @param scale the brightness value to use in place of the stored value
	void show(uint8_t scale);

	/// Update all our controllers with the current led colors
	void show() { show(m_Scale); }

	/// Clear the leds, wiping the local array of data. Optionally you can also
	/// send the cleared data to the LEDs.
	/// @param writeData whether or not to write out to the leds as well
	void clear(bool writeData = false);

	/// Clear out the local data array
	void clearData();

	/// Set all leds on all controllers to the given color/scale.
	/// @param color what color to set the leds to
	/// @param scale what brightness scale to show at
	void showColor(const struct CRGB & color, uint8_t scale);

	/// Set all leds on all controllers to the given color
	/// @param color what color to set the leds to
	void showColor(const struct CRGB & color) { showColor(color, m_Scale); }

	/// Delay for the given number of milliseconds.  Provided to allow the library to be used on platforms
	/// that don't have a delay function (to allow code to be more portable). 
	/// @note This will call show() constantly to drive the dithering engine (and will call show() at least once).
	/// @param ms the number of milliseconds to pause for
	void delay(unsigned long ms);

	/// Set a global color temperature.  Sets the color temperature for all added led strips,
	/// overriding whatever previous color temperature those controllers may have had.
	/// @param temp A CRGB structure describing the color temperature
	void setTemperature(const struct CRGB & temp);

	/// Set a global color correction.  Sets the color correction for all added led strips,
	/// overriding whatever previous color correction those controllers may have had.
	/// @param correction A CRGB structure describin the color correction.
	void setCorrection(const struct CRGB & correction);

	/// Set the dithering mode.  Sets the dithering mode for all added led strips, overriding
	/// whatever previous dithering option those controllers may have had.
	/// @param ditherMode what type of dithering to use, either BINARY_DITHER or DISABLE_DITHER
	void setDither(uint8_t ditherMode = BINARY_DITHER);

	/// Set the maximum refresh rate.  This is global for all leds.  Attempts to
	/// call show() faster than this rate will simply wait.
	/// @note The refresh rate defaults to the slowest refresh rate of all the leds added through addLeds().
	/// If you wish to set/override this rate, be sure to call setMaxRefreshRate() _after_
	/// adding all of your leds.
	/// @param refresh maximum refresh rate in hz
	/// @param constrain constrain refresh rate to the slowest speed yet set
	void setMaxRefreshRate(uint16_t refresh, bool constrain=false);

	/// For debugging, this will keep track of time between calls to countFPS(). Every
	/// `nFrames` calls, it will update an internal counter for the current FPS.
	/// @todo Make this a rolling counter
	/// @param nFrames how many frames to time for determining FPS
	void countFPS(int nFrames=25);

	/// Get the number of frames/second being written out
	/// @returns the most recently computed FPS value
	uint16_t getFPS() { return m_nFPS; }

	/// Get how many controllers have been registered
	/// @returns the number of controllers (strips) that have been added with addLeds()
	int count();

	/// Get a reference to a registered controller
	/// @returns a reference to the Nth controller
	CLEDController & operator[](int x);

	/// Get the number of leds in the first controller
	/// @returns the number of LEDs in the first controller
	int size();

	/// Get a pointer to led data for the first controller
	/// @returns pointer to the CRGB buffer for the first controller
	CRGB *leds();
};

/// Alias of the FastLED instance for legacy purposes
#define FastSPI_LED FastLED
/// Alias of the FastLED instance for legacy purposes
#define FastSPI_LED2 FastLED
#ifndef LEDS
/// Alias of the FastLED instance for legacy purposes
#define LEDS FastLED
#endif

/// Global LED strip management instance
extern CFastLED FastLED;

/// If no pin/port mappings are found, sends a warning message to the user
/// during compilation.
/// @see fastpin.h
#ifndef HAS_HARDWARE_PIN_SUPPORT
#warning "No pin/port mappings found, pin access will be slightly slower. See fastpin.h for info."
#define NO_HARDWARE_PIN_SUPPORT
#endif


FASTLED_NAMESPACE_END

#endif

#ifdef FASTLED_UI
// As a convenience, include the UI headers and bring them into the global namespace
#include "fl/ui.h"
using fl::UIButton;
using fl::UICheckbox;
using fl::UINumberField;
using fl::UISlider;
#define FASTLED_TITLE(text) fl::UITitle g_title(text)
#define FASTLED_DESCRIPTION(text) fl::UIDescription g_description(text)
#endif // FASTLED_UI

