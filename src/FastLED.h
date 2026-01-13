#pragma once
#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include "fl/stl/stdint.h"
#include "fl/dll.h"  // Will optionally compile in.

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
#define FASTLED_VERSION 3010003
#ifndef FASTLED_INTERNAL
#  ifdef  FASTLED_SHOW_VERSION
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "FastLED version 3.010.003"
#    else
#      warning FastLED version 3.010.003  (Not really a warning, just telling you here.)
#    endif
#  endif
#endif
// ESP32 I2S Driver compatibility warning
#if defined(FASTLED_ESP32_I2S) && !defined(FASTLED_INTERNAL)
#include "platforms/esp/esp_version.h"
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#warning "ESP32 I2S Driver: ESP-IDF 5.x detected. This driver may not work reliably with ESP-IDF 5.x+. Consider using RMT driver instead or staying on ESP-IDF 4.x. See ESP32_I2S_ISSUES.md for details. Report issues at: https://github.com/FastLED/FastLED/issues"
#endif
#endif

// ESP32 RMT Driver conflict prevention
// Arduino ESP32 Core 3.x (ESP-IDF 5.x+) includes a neopixelWrite() function that uses the RMT driver,
// which conflicts with FastLED's legacy RMT4 driver (FASTLED_RMT5=0).
// Disable Arduino's built-in RGB LED support to prevent the conflict.
// This is only needed for ESP-IDF 5.x+ with legacy RMT4 driver, not earlier versions or RMT5 driver.
// Only applies to platforms that actually have RMT hardware (excludes ESP32-C2 which lacks RMT).
// Reference: https://github.com/espressif/arduino-esp32/issues/9866
#if defined(ESP32)
#include "platforms/esp/esp_version.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_RMT5 == 0 && FASTLED_ESP32_HAS_RMT && !defined(ESP32_ARDUINO_NO_RGB_BUILTIN)
#error "ESP-IDF 5.x+ with legacy RMT4 driver detected: Please define ESP32_ARDUINO_NO_RGB_BUILTIN=1 to prevent conflicts with Arduino's neopixelWrite() function. Add '-DESP32_ARDUINO_NO_RGB_BUILTIN=1' to your build flags or use '-DFASTLED_RMT5=1' to enable RMT5 driver."
#endif
#endif


#if !defined(FASTLED_FAKE_SPI_FORWARDS_TO_FAKE_CLOCKLESS)
#if defined(__EMSCRIPTEN__)
#define FASTLED_FAKE_SPI_FORWARDS_TO_FAKE_CLOCKLESS 1
#else
#define FASTLED_FAKE_SPI_FORWARDS_TO_FAKE_CLOCKLESS 0
#endif
#endif

#ifndef __PROG_TYPES_COMPAT__
/// avr-libc define to expose __progmem__ typedefs.
/// @note These typedefs are now deprecated!
/// @see https://www.nongnu.org/avr-libc/user-manual/group__avr__pgmspace.html
#define __PROG_TYPES_COMPAT__
#endif

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/js.h"
#include "platforms/wasm/led_sysdefs_wasm.h"
#include "platforms/wasm/compiler/Arduino.h"
#include "platforms/wasm/js_fetch.h"
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

// Convenience includes for sketch inclusion
#include "fl/async.h"
#include "fl/sketch_macros.h"
#include "fl/rx_device.h"
#include "fl/stl/array.h"
#include "fl/stl/vector.h"
#include "fl/stl/cstring.h"


#include "fl/force_inline.h"
#include "cpp_compat.h"

#include "fastled_config.h"
#include "led_sysdefs.h"

// Include the internal FastLED header (provides core types without cycles)
#include "fl/fastled.h"

#include "fl/channels/channel.h"
#include "fl/channels/bus_manager.h"

// ============================================================================
// C STRING FUNCTION USING DECLARATIONS
// ============================================================================

/// Memory functions are available in fl:: namespace via fl/stl/cstring.h
/// Using declarations cannot work because system headers define memset/memcpy/memmove
/// before FastLED.h is fully processed, causing signature conflicts even though
/// fl::size and ::size_t refer to the same underlying type.
/// Use fl::memset, fl::memcpy, fl::memmove directly

// Utility functions
#include "fastled_delay.h"
#include "bitswap.h"

#include "lib8tion.h"

// ============================================================================
// MATH FUNCTION USING DECLARATIONS
// ============================================================================


#include "controller.h"
#include "fastpin.h"
#include "fastspi_types.h"
#include "dmx.h"

#include "platforms.h"
#include "fastled_progmem.h"
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

#include "fl/leds.h"

// clockless.h removed - BulkClockless API has been superseded by Channel API

// SPI chipset enumeration (modern type-safe enum class)
#include "fl/chipsets/spi_chipsets.h"

/// Backwards compatibility enum - allows old code to use unscoped names (e.g., APA102 instead of fl::SpiChipset::APA102)
/// @deprecated Use fl::SpiChipset enum class instead for type safety
enum ESPIChipsets {
	LPD6803   = static_cast<int>(fl::SpiChipset::LPD6803),   ///< LPD6803 LED chipset
	LPD8806   = static_cast<int>(fl::SpiChipset::LPD8806),   ///< LPD8806 LED chipset
	WS2801    = static_cast<int>(fl::SpiChipset::WS2801),    ///< WS2801 LED chipset
	WS2803    = static_cast<int>(fl::SpiChipset::WS2803),    ///< WS2803 LED chipset
	SM16716   = static_cast<int>(fl::SpiChipset::SM16716),   ///< SM16716 LED chipset
	P9813     = static_cast<int>(fl::SpiChipset::P9813),     ///< P9813 LED chipset
	APA102    = static_cast<int>(fl::SpiChipset::APA102),    ///< APA102 LED chipset
	SK9822    = static_cast<int>(fl::SpiChipset::SK9822),    ///< SK9822 LED chipset
	SK9822HD  = static_cast<int>(fl::SpiChipset::SK9822HD),  ///< SK9822 LED chipset with 5-bit gamma correction
	DOTSTAR   = static_cast<int>(fl::SpiChipset::DOTSTAR),   ///< APA102 LED chipset alias
	DOTSTARHD = static_cast<int>(fl::SpiChipset::DOTSTARHD), ///< APA102HD LED chipset alias
	APA102HD  = static_cast<int>(fl::SpiChipset::APA102HD),  ///< APA102 LED chipset with 5-bit gamma correction
	HD107     = static_cast<int>(fl::SpiChipset::HD107),     ///< Same as APA102, but in turbo 40-mhz mode.
	HD107HD   = static_cast<int>(fl::SpiChipset::HD107HD),   ///< Same as APA102HD, but in turbo 40-mhz mode.
	HD108     = static_cast<int>(fl::SpiChipset::HD108),     ///< 16-bit variant of HD107, always gamma corrected. No SD (standard definition) option available - all HD108s use gamma correction, and a non-gamma corrected version is not planned.
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
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER> class PIXIE : public PixieController<DATA_PIN, RGB_ORDER> {};
#endif

#ifdef FL_CLOCKLESS_CONTROLLER_DEFINED
/// @addtogroup Chipsets
/// @{
/// @addtogroup ClocklessChipsets
/// @{

/// LED controller for WS2812 LEDs with GRB color order
/// @see WS2812Controller800Khz
template<fl::u8 DATA_PIN> class NEOPIXEL : public WS2812Controller800Khz<DATA_PIN, GRB> {};

/// @brief SM16703 controller class.
/// @copydetails SM16703Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER> 
class SM16703 : public SM16703Controller<DATA_PIN, RGB_ORDER> {};

/// @brief SM16824E controller class.
/// @copydetails SM16824EController
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER> 
class SM16824E : public SM16824EController<DATA_PIN, RGB_ORDER> {};

/// @brief TM1829 controller class.
/// @copydetails TM1829Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class TM1829 : public TM1829Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1812 controller class.
/// @copydetails TM1809Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class TM1812 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1809 controller class.
/// @copydetails TM1809Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class TM1809 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1804 controller class.
/// @copydetails TM1809Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class TM1804 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief TM1803 controller class.
/// @copydetails TM1803Controller400Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class TM1803 : public TM1803Controller400Khz<DATA_PIN, RGB_ORDER> {}; 

/// @brief UCS1903 controller class.
/// @copydetails UCS1903Controller400Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS1903 : public UCS1903Controller400Khz<DATA_PIN, RGB_ORDER> {};

/// @brief UCS1903B controller class.
/// @copydetails UCS1903BController800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS1903B : public UCS1903BController800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief UCS1904 controller class.
/// @copydetails UCS1904Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS1904 : public UCS1904Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief UCS2903 controller class.
/// @copydetails UCS2903Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS2903 : public UCS2903Controller<DATA_PIN, RGB_ORDER> {};

/// @brief UCS7604 controller class (8-bit @ 800kHz).
/// @copydetails UCS7604Controller8bit
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS7604 : public UCS7604Controller8bit<DATA_PIN, RGB_ORDER> {};

/// @brief UCS7604HD controller class (16-bit @ 800kHz).
/// @copydetails UCS7604Controller16bit
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS7604HD : public UCS7604Controller16bit<DATA_PIN, RGB_ORDER> {};

/// @brief UCS7604HD_1600 controller class (16-bit @ 1600kHz).
/// High-speed variant of UCS7604HD with twice the data rate
/// @copydetails UCS7604Controller16bit1600
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS7604HD_1600 : public UCS7604Controller16bit1600<DATA_PIN, RGB_ORDER> {};

/// @brief WS2812 controller class.
/// @copydetails WS2812Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2812 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2815 controller class.
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2815 : public WS2815Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2816 controller class.
template <fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2816 : public WS2816Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2852 controller class.
/// @copydetails WS2812Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2852 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2812B controller class.
/// @copydetails WS2812Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2812B : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2812B-Mini-V3 controller class.
/// @copydetails WS2812BMiniV3Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2812BMiniV3 : public WS2812BMiniV3Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2812B-V5 controller class (uses same timing as Mini-V3).
/// @copydetails WS2812BV5Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2812BV5 : public WS2812BV5Controller<DATA_PIN, RGB_ORDER> {};

/// @brief GS1903 controller class.
/// @copydetails WS2812Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class GS1903 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief SK6812 controller class.
/// @copydetails SK6812Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class SK6812 : public SK6812Controller<DATA_PIN, RGB_ORDER> {};

/// @brief SK6822 controller class.
/// @copydetails SK6822Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class SK6822 : public SK6822Controller<DATA_PIN, RGB_ORDER> {};

/// @brief APA106 controller class.
/// @copydetails SK6822Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class APA106 : public SK6822Controller<DATA_PIN, RGB_ORDER> {};

/// @brief PL9823 controller class.
/// @copydetails PL9823Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class PL9823 : public PL9823Controller<DATA_PIN, RGB_ORDER> {};

/// @brief WS2811 controller class.
/// @copydetails WS2811Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2811 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2813 controller class.
/// @copydetails WS2813Controller
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2813 : public WS2813Controller<DATA_PIN, RGB_ORDER> {};

/// @brief APA104 controller class.
/// @copydetails WS2811Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class APA104 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief WS2811_400 controller class.
/// @copydetails WS2811Controller400Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class WS2811_400 : public WS2811Controller400Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GE8822 controller class.
/// @copydetails GE8822Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class GE8822 : public GE8822Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GW6205 controller class.
/// @copydetails GW6205Controller800Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class GW6205 : public GW6205Controller800Khz<DATA_PIN, RGB_ORDER> {};

/// @brief GW6205_400 controller class.
/// @copydetails GW6205Controller400Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class GW6205_400 : public GW6205Controller400Khz<DATA_PIN, RGB_ORDER> {};

/// @brief LPD1886 controller class.
/// @copydetails LPD1886Controller1250Khz
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class LPD1886 : public LPD1886Controller1250Khz<DATA_PIN, RGB_ORDER> {};

/// @brief LPD1886_8BIT controller class.
/// @copydetails LPD1886Controller1250Khz_8bit
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class LPD1886_8BIT : public LPD1886Controller1250Khz_8bit<DATA_PIN, RGB_ORDER> {};

/// @brief UCS1912 controller class.
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
class UCS1912 : public UCS1912Controller<DATA_PIN, RGB_ORDER> {};

#if defined(DmxSimple_h) || defined(FASTLED_DOXYGEN)
/// @copydoc DMXSimpleController
template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER> class DMXSIMPLE : public DMXSimpleController<DATA_PIN, RGB_ORDER> {};
#endif
#if defined(DmxSerial_h) || defined(FASTLED_DOXYGEN)
/// @copydoc DMXSerialController
template<fl::EOrder RGB_ORDER> class DMXSERIAL : public DMXSerialController<RGB_ORDER> {};
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
typedef fl::u8 (*power_func)(fl::u8 scale, fl::u32 data);

/// High level controller interface for FastLED.
/// This class manages controllers, global settings, and trackings such as brightness
/// and refresh rates, and provides access functions for driving led data to controllers
/// via the show() / showColor() / clear() methods.
/// This is instantiated as a global object with the name FastLED.
/// @nosubgrouping
class CFastLED {
	// int m_nControllers;
	fl::u8  m_Scale;         ///< the current global brightness scale setting
	        fl::u16 m_nFPS;          ///< tracking for current frames per second (FPS) value
	fl::u32 m_nMinMicros;    ///< minimum µs between frames, used for capping frame rates
	fl::u32 m_nPowerData;    ///< max power use parameter
	power_func m_pPowerFunc;  ///< function for overriding brightness when using FastLED.show();

public:
	CFastLED();

	/// Initialize platform-specific subsystems
	///
	/// Performs one-time initialization of platform-specific hardware and subsystems.
	/// This is called automatically on first use but can be called explicitly to ensure
	/// initialization happens at a predictable time.
	///
	/// Safe to call multiple times - subsequent calls are no-ops.
	///
	/// Platform-specific initialization:
	/// - ESP32: Initializes channel bus manager (PARLIO/SPI/RMT/UART) and SPI bus manager
	/// - Other platforms: No-op (no initialization required)
	void init();

	// Useful when you want to know when an event like onFrameBegin or onFrameEnd is happening.
	// This is disabled on AVR to save space.
	void addListener(fl::EngineEvents::Listener *listener) { fl::EngineEvents::addListener(listener); }
	void removeListener(fl::EngineEvents::Listener *listener) { fl::EngineEvents::removeListener(listener); }

	/// @name Manual Engine Events
	/// When FASTLED_MANUAL_ENGINE_EVENTS is defined, these methods allow manual control of engine events.
	/// When FASTLED_MANUAL_ENGINE_EVENTS is not defined, these events are triggered automatically by show().
	/// @{
	
	/// Manually trigger the begin frame event
	/// @note This is called automatically by show() unless FASTLED_MANUAL_ENGINE_EVENTS is defined
	void onBeginFrame() ;
	
	/// Manually trigger the end show LEDs event
	/// @note This is called automatically by show() unless FASTLED_MANUAL_ENGINE_EVENTS is defined
	void onEndShowLeds() ;
	
	/// @} Manual Engine Events

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
	static ::CLEDController &addLeds(::CLEDController *pLed, CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

	/// @brief Add a Channel-based LED controller to the world
	///
	/// This method registers a Channel instance (created via Channel::create<ENGINE>(config))
	/// with the FastLED controller list. Channels provide hardware-accelerated parallel LED output
	/// using platform-specific engines (e.g., ESP32 PARLIO, Teensy FlexIO).
	///
	/// Example usage:
	/// @code
	/// // Create channel configuration
	/// ChannelConfig config(pin, timing, leds, RGB);
	///
	/// // Create and register channel with specific engine
	/// ChannelPtr channel = FastLED.addLedChannel(Channel::create<fl::ChannelEnginePARLIO>(config));
	/// @endcode
	///
	/// @param channel Shared pointer to a Channel instance
	/// @returns The same ChannelPtr for chaining or storage
	static fl::ChannelPtr addLedChannel(fl::ChannelPtr channel) {
		// Channel inherits from CLEDController, so it's already in the linked list
		// No additional registration needed beyond what Channel::create() does
		return channel;
	}

	/// @name Runtime LED Channel Management
	/// @{

	/// Add LED channel with runtime configuration
	///
	/// Creates a Channel-based LED controller with runtime-configurable timing.
	/// Unlike the template-based addLeds<>() which uses static storage, this
	/// function returns a shared_ptr that allows the controller to be destroyed
	/// and recreated with different configurations.
	///
	/// @param config Channel configuration (pin, timing, leds, rgb order, settings)
	/// @returns Shared pointer to Channel (extends CLEDController), or nullptr if unsupported
	/// @note Supported platforms: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-P4
	/// @note Controller auto-registers on creation, auto-removes on destruction
	/// @note MUST store return value to control lifetime - marked [[nodiscard]]
	///
	/// Example:
	/// @code
	/// auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
	/// fl::ChannelConfig config(PIN_DATA, timing, fl::span<CRGB>(leds, NUM_LEDS), RGB);
	/// auto channel = FastLED.addChannel(config);
	/// fill_solid(leds, NUM_LEDS, CRGB::Red);
	/// FastLED.show();
	/// channel.reset();  // Destroy controller
	/// @endcode
	FL_NODISCARD static fl::ChannelPtr addChannel(const fl::ChannelConfig& config);

	/// @}

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
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN>
	struct ClockedChipsetHelper {
	    // Default implementation, will be specialized for supported chipsets
		static const bool IS_VALID = false;
	};

	// Macro to define a mapping from the ESPIChipeset enum to the controller class
	// in it's various template configurations.
	#define _FL_MAP_CLOCKED_CHIPSET(CHIPSET_ENUM, CONTROLLER_CLASS)                              \
		template<fl::u8 DATA_PIN, fl::u8 CLOCK_PIN>                                              \
		struct ClockedChipsetHelper<CHIPSET_ENUM, DATA_PIN, CLOCK_PIN> {                           \
		    static const bool IS_VALID = true;                                                     \
			typedef CONTROLLER_CLASS<DATA_PIN, CLOCK_PIN> ControllerType;                          \
			/* Controller type with RGB_ORDER specified */                                         \
			template<fl::EOrder RGB_ORDER>															   \
			struct CONTROLLER_CLASS_WITH_ORDER {                                                   \
				typedef CONTROLLER_CLASS<DATA_PIN, CLOCK_PIN, RGB_ORDER> ControllerType;           \
			};                                                                                     \
			/* Controller type with RGB_ORDER and spi frequency specified */                       \
			template<fl::EOrder RGB_ORDER, fl::u32 FREQ>                                              \
			struct CONTROLLER_CLASS_WITH_ORDER_AND_FREQ {                                          \
				typedef CONTROLLER_CLASS<DATA_PIN, CLOCK_PIN, RGB_ORDER, FREQ> ControllerType;     \
			};                                                                                     \
		};

	// Define specializations for each supported CHIPSET
	// Note: Using unscoped enum values for backwards compatibility
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

	_FL_MAP_CLOCKED_CHIPSET(HD108, HD108Controller)

	_FL_MAP_CLOCKED_CHIPSET(SK9822, SK9822Controller)
	_FL_MAP_CLOCKED_CHIPSET(SK9822HD, SK9822ControllerHD)


	#if FASTLED_FAKE_SPI_FORWARDS_TO_FAKE_CLOCKLESS
	/// Stubbed out platforms have unique challenges in faking out the SPI based controllers.
	/// Therefore for these platforms we will always delegate to the WS2812 clockless controller.
	/// This is fine because the clockless controllers on the stubbed out platforms are fake anyways.
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::EOrder RGB_ORDER, fl::u32 SPI_DATA_RATE > ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		// Instantiate the controller using ClockedChipsetHelper
		// Always USE WS2812 clockless controller since it's the common path.
		return addLeds<WS2812, DATA_PIN, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add an SPI based CLEDController instance to the world.
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN > static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		// Always USE WS2812 clockless controller since it's the common path.
		return addLeds<WS2812, DATA_PIN>(data, nLedsOrOffset, nLedsIfOffset);
	}


	// The addLeds function using ChipsetHelper
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::EOrder RGB_ORDER>
	::CLEDController& addLeds(CRGB* data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		// Always USE WS2812 clockless controller since it's the common path.
		return addLeds<WS2812, DATA_PIN, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}

	#else


	/// Add an SPI based CLEDController instance to the world.
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::EOrder RGB_ORDER, fl::u32 SPI_DATA_RATE > ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		// Instantiate the controller using ClockedChipsetHelper
		typedef ClockedChipsetHelper<CHIPSET, DATA_PIN, CLOCK_PIN> CHIP;
		typedef typename CHIP::template CONTROLLER_CLASS_WITH_ORDER_AND_FREQ<RGB_ORDER, SPI_DATA_RATE>::ControllerType ControllerTypeWithFreq;
		static_assert(CHIP::IS_VALID, "Unsupported chipset");
		static ControllerTypeWithFreq c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add an SPI based CLEDController instance to the world.
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN > static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		typedef ClockedChipsetHelper<CHIPSET, DATA_PIN, CLOCK_PIN> CHIP;
		typedef typename CHIP::ControllerType ControllerType;
		static_assert(CHIP::IS_VALID, "Unsupported chipset");
		static ControllerType c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}


	// The addLeds function using ChipsetHelper
	template<ESPIChipsets CHIPSET, fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::EOrder RGB_ORDER>
	::CLEDController& addLeds(CRGB* data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		typedef ClockedChipsetHelper<CHIPSET, DATA_PIN, CLOCK_PIN> CHIP;
		static_assert(CHIP::IS_VALID, "Unsupported chipset");
		typedef typename CHIP::template CONTROLLER_CLASS_WITH_ORDER<RGB_ORDER>::ControllerType ControllerTypeWithOrder;
		static ControllerTypeWithOrder c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}
	#endif


#ifdef SPI_DATA
	template<ESPIChipsets CHIPSET> static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, fl::EOrder RGB_ORDER> static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, fl::EOrder RGB_ORDER, fl::u32 SPI_DATA_RATE> static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER, SPI_DATA_RATE>(data, nLedsOrOffset, nLedsIfOffset);
	}

#endif
	/// @} Adding SPI based controllers

#ifdef FL_CLOCKLESS_CONTROLLER_DEFINED
	/// @brief Helper to unwrap controller pointer and call base addLeds
	/// This explicit helper function aids AVR GCC with template-dependent type resolution
	/// during two-phase name lookup, solving compilation issues on older compiler versions.
	template<typename ControllerType>
	static inline ::CLEDController& addLedsImpl(ControllerType* controller, CRGB *data, int nLedsOrOffset, int nLedsIfOffset) {
		::CLEDController* pLed = static_cast<::CLEDController*>(controller);
		return addLeds(pLed, data, nLedsOrOffset, nLedsIfOffset);
	}
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
	template<template<fl::u8, fl::EOrder> class CHIPSET, fl::u8 DATA_PIN, fl::EOrder RGB_ORDER>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB_ORDER> c;
		return addLedsImpl(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add a clockless based CLEDController instance to the world.
	template<template<fl::u8, fl::EOrder> class CHIPSET, fl::u8 DATA_PIN>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB> c;
		return addLedsImpl(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add a clockless based CLEDController instance to the world.
	template<template<fl::u8> class CHIPSET, fl::u8 DATA_PIN>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN> c;
		return addLedsImpl(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<fl::u8> class CHIPSET, fl::u8 DATA_PIN>
	static ::CLEDController &addLeds(fl::Leds& leds, int nLedsOrOffset, int nLedsIfOffset = 0) {
		CRGB* rgb = leds;
		return addLeds<CHIPSET, DATA_PIN>(rgb, nLedsOrOffset, nLedsIfOffset);
	}

#if defined(__FASTLED_HAS_FIBCC) && (__FASTLED_HAS_FIBCC == 1)
	template<fl::u8 NUM_LANES, template<fl::u8 DATA_PIN, fl::EOrder RGB_ORDER> class CHIPSET, fl::u8 DATA_PIN, fl::EOrder RGB_ORDER=RGB>
	static ::CLEDController &addLeds(CRGB *data, int nLeds) {
		static fl::__FIBCC<CHIPSET, DATA_PIN, NUM_LANES, RGB_ORDER> c;
		return addLeds(&c, data, nLeds);
	}
#endif

	#ifdef FASTSPI_USE_DMX_SIMPLE
	template<EClocklessChipsets CHIPSET, fl::u8 DATA_PIN, fl::EOrder RGB_ORDER=RGB>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
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
	template<template<fl::EOrder RGB_ORDER> class CHIPSET, fl::EOrder RGB_ORDER>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	/// Add a 3rd party library based CLEDController instance to the world.
	template<template<fl::EOrder RGB_ORDER> class CHIPSET>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

#ifdef USE_OCTOWS2811
	/// Add a OCTOWS2811 based CLEDController instance to the world.
	/// @see https://www.pjrc.com/teensy/td_libs_OctoWS2811.html
	/// @see https://github.com/PaulStoffregen/OctoWS2811
	template<OWS2811 CHIPSET, fl::EOrder RGB_ORDER>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case OCTOWS2811: { static fl::COctoWS2811Controller<RGB_ORDER,WS2811_800kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case OCTOWS2811_400: { static fl::COctoWS2811Controller<RGB_ORDER,WS2811_400kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#ifdef WS2813_800kHz
      case OCTOWS2813: { static fl::COctoWS2811Controller<RGB_ORDER,WS2813_800kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#endif
		}
	}

	/// Add a OCTOWS2811 library based CLEDController instance to the world.
	/// @see https://www.pjrc.com/teensy/td_libs_OctoWS2811.html
	/// @see https://github.com/PaulStoffregen/OctoWS2811
	template<OWS2811 CHIPSET>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		return addLeds<CHIPSET,GRB>(data,nLedsOrOffset,nLedsIfOffset);
	}

#endif

#ifdef USE_WS2812SERIAL
	/// Add a WS2812Serial library based CLEDController instance to the world.
	/// @see https://www.pjrc.com/non-blocking-ws2812-led-library/
	/// @see https://github.com/PaulStoffregen/WS2812Serial
	template<SWS2812 CHIPSET, int DATA_PIN, fl::EOrder RGB_ORDER>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		static CWS2812SerialController<DATA_PIN,RGB_ORDER> controller;
		return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset);
	}
#endif

#ifdef SmartMatrix_h
	/// Add a SmartMatrix library based CLEDController instance to the world.
	/// @see https://github.com/pixelmatix/SmartMatrix
	template<ESM CHIPSET>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
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
	template<EBlockChipsets CHIPSET, int NUM_LANES, fl::EOrder RGB_ORDER>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
		#ifdef PORTA_FIRST_PIN
				case WS2811_PORTA: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, fl::TIMING_WS2811_800KHZ_LEGACY, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTA: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, fl::TIMING_WS2811_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTA: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, fl::TIMING_WS2813, RGB_ORDER, 0, false>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTA: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, fl::TIMING_TM1803_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTA: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, fl::TIMING_UCS1903_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTB_FIRST_PIN
				case WS2811_PORTB: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, fl::TIMING_WS2811_800KHZ_LEGACY, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTB: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, fl::TIMING_WS2811_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTB: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, fl::TIMING_WS2813, RGB_ORDER, 0, false>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTB: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, fl::TIMING_TM1803_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTB: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, fl::TIMING_UCS1903_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTC_FIRST_PIN
				case WS2811_PORTC: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, fl::TIMING_WS2811_800KHZ_LEGACY, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTC: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, fl::TIMING_WS2811_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTC: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, fl::TIMING_WS2813, RGB_ORDER, 0, false>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTC: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, fl::TIMING_TM1803_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTC: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, fl::TIMING_UCS1903_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTD_FIRST_PIN
				case WS2811_PORTD: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, fl::TIMING_WS2811_800KHZ_LEGACY, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTD: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, fl::TIMING_WS2811_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTD: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, fl::TIMING_WS2813, RGB_ORDER, 0, false>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTD: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, fl::TIMING_TM1803_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTD: return addLeds(new fl::InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, fl::TIMING_UCS1903_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef HAS_PORTDC
				case WS2811_PORTDC: return addLeds(new fl::SixteenWayInlineBlockClocklessController<NUM_LANES, fl::TIMING_WS2811_800KHZ_LEGACY, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTDC: return addLeds(new fl::SixteenWayInlineBlockClocklessController<NUM_LANES, fl::TIMING_WS2811_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTDC: return addLeds(new fl::SixteenWayInlineBlockClocklessController<NUM_LANES, fl::TIMING_WS2813, RGB_ORDER, 0, false>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTDC: return addLeds(new fl::SixteenWayInlineBlockClocklessController<NUM_LANES, fl::TIMING_TM1803_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTDC: return addLeds(new fl::SixteenWayInlineBlockClocklessController<NUM_LANES, fl::TIMING_UCS1903_400KHZ, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		}
	}

	/// Add a block based parallel output CLEDController instance to the world.
	template<EBlockChipsets CHIPSET, int NUM_LANES>
	static ::CLEDController &addLeds(CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET,NUM_LANES,GRB>(data,nLedsOrOffset,nLedsIfOffset);
	}
	/// @} Adding parallel output controllers
#endif

	/// Set the global brightness scaling
	/// @param scale a 0-255 value for how much to scale all leds before writing them out
	void setBrightness(fl::u8 scale) { m_Scale = scale; }

	/// Get the current global brightness setting
	/// @returns the current global brightness value
	fl::u8 getBrightness() { return m_Scale; }

	/// @name Channel Bus Manager Controls
	/// Configure platform-specific channel bus drivers
	/// @{

#ifdef ESP32
	/// @platform ESP32
	/// @note These methods control driver selection for RMT, SPI, and PARLIO engines
	/// @note Only functional on ESP32 platforms with multi-engine architecture

	/// Enable or disable a channel driver by name at runtime
	/// @param name Driver name to control (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
	/// @param enabled true to enable, false to disable
	/// @note Disabled drivers are skipped during selection
	/// @note Changes take effect immediately on next LED update
	void setDriverEnabled(const char* name, bool enabled);

	/// Enable only one driver exclusively (disables all others)
	/// @param name Driver name to enable exclusively (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
	/// @return true if driver was found and set as exclusive, false if name not found
	/// @note Atomically disables all drivers, then enables the specified one
	/// @note Use for testing specific drivers or debugging
	bool setExclusiveDriver(const char* name);

	/// Check if a driver is enabled by name
	/// @param name Driver name to query (case-sensitive)
	/// @return true if enabled, false if disabled or not registered
	bool isDriverEnabled(const char* name) const;

	/// Get count of registered channel drivers
	/// @return Total number of registered engines (including unnamed ones)
	fl::size getDriverCount() const;

	/// Get full state of all registered channel drivers
	/// @return Span of driver info (sorted by priority descending)
	/// @note Returned span is valid until next call to any non-const method
	fl::span<const fl::DriverInfo> getDriverInfos() const;
#else
	/// @platform Non-ESP32
	/// @warning These methods are no-op stubs on non-ESP32 platforms
	/// @note Non-ESP32 platforms use single-engine architectures (no runtime switching)

	/// No-op stub: Enable or disable a channel driver by name at runtime
	/// @param name Driver name (ignored on non-ESP32)
	/// @param enabled Enable flag (ignored on non-ESP32)
	/// @note Non-ESP32: Does nothing (safe no-op)
	void setDriverEnabled(const char* /*name*/, bool /*enabled*/) {
		// No-op: Only ESP32 has multi-engine architecture
	}

	/// No-op stub: Enable only one driver exclusively
	/// @param name Driver name (ignored on non-ESP32)
	/// @return Always returns false (no engines available)
	/// @note Non-ESP32: Always returns false
	bool setExclusiveDriver(const char* /*name*/) {
		// No-op: Only ESP32 has multi-engine architecture
		return false;  // Name not found (no engines exist)
	}

	/// No-op stub: Check if a driver is enabled by name
	/// @param name Driver name (ignored on non-ESP32)
	/// @return Always returns false (no engines registered)
	/// @note Non-ESP32: Always returns false
	bool isDriverEnabled(const char* /*name*/) const {
		// No-op: Only ESP32 has multi-engine architecture
		return false;  // Name not found (no engines exist)
	}

	/// No-op stub: Get count of registered channel drivers
	/// @return Always returns 0 (no engines)
	/// @note Non-ESP32: Always returns 0
	fl::size getDriverCount() const {
		// No-op: Only ESP32 has multi-engine architecture
		return 0;  // No engines registered
	}

	/// No-op stub: Get full state of all registered channel drivers
	/// @return Empty span (no engines)
	/// @note Non-ESP32: Always returns empty span
	fl::span<const fl::DriverInfo> getDriverInfos() const {
		// No-op: Only ESP32 has multi-engine architecture
		return fl::span<const fl::DriverInfo>(nullptr, 0);  // Empty span
	}
#endif

	/// @} Channel Bus Manager Controls

	/// Wait for all channel bus transmissions to complete
	/// @note Polls the channel bus manager until it returns READY state
	/// @note Uses delayMicroseconds(100) between polls to prevent watchdog timeout
	/// @note Safe to call on all platforms (no-op on platforms without channel bus)
	void wait();

	/// Set the maximum power to be used, given in volts and milliamps.
	/// @param volts how many volts the leds are being driven at (usually 5)
	/// @param milliamps the maximum milliamps of power draw you want
	inline void setMaxPowerInVoltsAndMilliamps(fl::u8 volts, fl::u32 milliamps) { setMaxPowerInMilliWatts(volts * milliamps); }

	/// Set the maximum power to be used, given in milliwatts
	/// @param milliwatts the max power draw desired, in milliwatts
	inline void setMaxPowerInMilliWatts(fl::u32 milliwatts) { m_pPowerFunc = static_cast<power_func>(&calculate_max_brightness_for_power_mW); m_nPowerData = milliwatts; }

	/// @name Power Model Configuration
	/// Configure LED power consumption for accurate power management
	/// @{

	/// Set custom RGB LED power consumption model
	/// @param model RGB power consumption model
	/// @example FastLED.setPowerModel(PowerModelRGB(40, 40, 40, 2));
	inline void setPowerModel(const PowerModelRGB& model) {
		set_power_model(model);
	}

	/// Set custom RGBW LED power consumption model
	/// @param model RGBW power consumption model
	/// @note Future API enhancement - currently uses RGB components only
	/// @example FastLED.setPowerModel(PowerModelRGBW(90, 70, 90, 100, 5));
	inline void setPowerModel(const PowerModelRGBW& model) {
		set_power_model(model);
	}

	/// Set custom RGBWW LED power consumption model
	/// @param model RGBWW power consumption model
	/// @note Future API enhancement - currently uses RGB components only
	/// @example FastLED.setPowerModel(PowerModelRGBWW(85, 65, 85, 95, 95, 5));
	inline void setPowerModel(const PowerModelRGBWW& model) {
		set_power_model(model);
	}

	/// Get current RGB power model
	/// @returns Current RGB power consumption model
	inline PowerModelRGB getPowerModel() {
		return get_power_model();
	}

	/// @} Power Model Configuration

	/// Update all our controllers with the current led colors, using the passed in brightness
	/// @param scale the brightness value to use in place of the stored value
	void show(fl::u8 scale);

	/// Update all our controllers with the current led colors
	void show() { show(m_Scale); }

	// Called automatically at the end of show().
	void onEndFrame();

	/// Clear the leds, wiping the local array of data. Optionally you can also
	/// send the cleared data to the LEDs.
	/// @param writeData whether or not to write out to the leds as well
	void clear(bool writeData = false);

	/// Clear out the local data array
	void clearData();

	/// Set all leds on all controllers to the given color/scale.
	/// @param color what color to set the leds to
	/// @param scale what brightness scale to show at
	void showColor(const CRGB & color, fl::u8 scale);

	/// Set all leds on all controllers to the given color
	/// @param color what color to set the leds to
	void showColor(const CRGB & color) { showColor(color, m_Scale); }

	/// Delay for the given number of milliseconds.  Provided to allow the library to be used on platforms
	/// that don't have a delay function (to allow code to be more portable). 
	/// @note This will call show() constantly to drive the dithering engine (and will call show() at least once).
	/// @param ms the number of milliseconds to pause for
	void delay(unsigned long ms);

	/// Set a global color temperature.  Sets the color temperature for all added led strips,
	/// overriding whatever previous color temperature those controllers may have had.
	/// @param temp A CRGB structure describing the color temperature
	void setTemperature(const CRGB & temp);

	/// Set a global color correction.  Sets the color correction for all added led strips,
	/// overriding whatever previous color correction those controllers may have had.
	/// @param correction A CRGB structure describin the color correction.
	void setCorrection(const CRGB & correction);

	/// Set the dithering mode.  Sets the dithering mode for all added led strips, overriding
	/// whatever previous dithering option those controllers may have had.
	/// @param ditherMode what type of dithering to use, either BINARY_DITHER or DISABLE_DITHER
	void setDither(fl::u8 ditherMode = BINARY_DITHER);

	/// Set the maximum refresh rate.  This is global for all leds.  Attempts to
	/// call show() faster than this rate will simply wait.
	/// @note The refresh rate defaults to the slowest refresh rate of all the leds added through addLeds().
	/// If you wish to set/override this rate, be sure to call setMaxRefreshRate() _after_
	/// adding all of your leds.
	/// @param refresh maximum refresh rate in hz
	/// @param constrain constrain refresh rate to the slowest speed yet set
	        void setMaxRefreshRate(fl::u16 refresh, bool constrain=false);

	/// For debugging, this will keep track of time between calls to countFPS(). Every
	/// `nFrames` calls, it will update an internal counter for the current FPS.
	/// @todo Make this a rolling counter
	/// @param nFrames how many frames to time for determining FPS
	void countFPS(int nFrames=25);

	/// Get the number of frames/second being written out
	/// @returns the most recently computed FPS value
	        fl::u16 getFPS() { return m_nFPS; }

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


/// Global LED strip management instance
extern CFastLED FastLED;

/// If no pin/port mappings are found, sends a warning message to the user
/// during compilation.
/// @see fastpin.h
#ifndef HAS_HARDWARE_PIN_SUPPORT
#warning "No pin/port mappings found, pin access will be slightly slower. See fastpin.h for info."
#define NO_HARDWARE_PIN_SUPPORT
#endif


#endif


/////////////////////////// Convenience includes for sketches ///////////////////////////

#if !defined(FASTLED_INTERNAL) && !defined(FASTLED_LEAN_AND_MEAN)

#include "fl/str.h"   // Awesome Str class that has stack allocation and heap overflow, copy on write.
#include "fl/xymap.h"  // XYMap class for mapping 2D coordinates on seperintine matrices.

#include "fl/clamp.h"  // fl::clamp(value, min, max)
#include "fl/map_range.h"  // fl::map_range(value, in_min, in_max, out_min, out_max)

#include "fl/error.h"
#include "fl/warn.h"  // FASTLED_WARN("time now: " << millis()), FASTLED_WARN_IF(condition, "time now: " << millis());"
#include "fl/log.h"  // FL_PRINT("message" << value), FL_LOG_*() category-specific logging
#include "fl/stl/assert.h"  // FASTLED_ASSERT(condition, "message");
#include "fl/unused.h"  // FASTLED_UNUSED(variable), for strict compiler settings.
#include "fl/stl/sstream.h"  // fl::sstream for string stream operations
#include "fl/remote.h"  // Remote RPC system for JSON-based function calls (requires FASTLED_ENABLE_JSON)

// provides:
//   fl::vector<T> - Standard heap vector
//   fl::vector_inlined<T,N> - Allocate on stack N elements, then overflow to heap vector.
//   fl::vector_fixed<T,N> - Stack allocated fixed size vector, elements will fail to add when full.
#include "fl/stl/vector.h"

// Flexible callbacks in the style of std::function.
#include "fl/stl/function.h"

// Clears the led data and other objects.
// CRGB leds[NUM_LEDS];
// fl::clear(leds)
#include "fl/clear.h"

// Leds has a CRGB block and an XYMap
#include "fl/leds.h"

#include "fl/spi.h"  // SPI device and multi-lane SPI support (1-16 lanes)

#include "fl/ui.h"  // Provides UIButton, UISlider, UICheckbox, UINumberField and UITitle, UIDescription, UIHelp, UIGroup.
using fl::UITitle;
using fl::UIDescription;
using fl::UIHelp;
using fl::UIButton;  // These names are unique enough that we don't need to namespace them
using fl::UICheckbox;
using fl::UINumberField;
using fl::UISlider;
using fl::UIDropdown;
using fl::UIGroup;
using fl::XYMap;
using fl::round;  // Template version avoids conflicts with ::round

// Common fl:: type aliases for global namespace convenience
template<typename T> using fl_vector = fl::vector<T>;
template<typename Key, typename Value, typename Compare = fl::less<Key>> using fl_map = fl::fl_map<Key, Value, Compare>;
using fl_string = fl::string;

// Note: delayMicroseconds and delayMillis are provided by Arduino core
// The fl:: namespace versions are available for explicit use when needed

#define FASTLED_TITLE(text) fl::UITitle g_title(text)
#define FASTLED_DESCRIPTION(text) fl::UIDescription g_description(text)
#define FASTLED_HELP(text) fl::UIHelp g_help(text)


#endif // FASTLED_INTERNAL && !FASTLED_LEAN_AND_MEAN



// Experimental: loop() hijacking.
//
// EngineEvents requires that FastLED.show() be invoked.
// If the user skips that then certain updates will be skipped.
//
// Right now this isn't a big deal, but in the future it could be.
//
// Therefore this experiment is done so that this loop() hijack trick
// can be used to insert code at the start of every loop(), such as a
// scoped object that forces a begin and end frame event.
//
// It's possible to hijack the loop() via a macro so that
// extra code can be injected at the start of every frame.
// When FASTLED_LOOP_RUNS_ASYNC == 1
// then use loop() to also run the async.
// 
// EXAMPLE:
// #define FASTLED_LOOP_RUNS_ASYNC 1
// #include "FastLED.h"
// void loop() {
//     FASTLED_WARN("async will run in the loop");
//     delay(500);
// }

#ifndef FASTLED_LOOP_RUNS_ASYNC
#define FASTLED_LOOP_RUNS_ASYNC 0
#endif

#if FASTLED_LOOP_RUNS_ASYNC == 1
// The loop is set as a macro that re-defines the user loop function
// to sketch_loop()
#define loop() \
     sketch_loop(); \
     void loop() { sketch_loop(); fl::async_run(); } \
     void sketch_loop()
#endif

// Backdoor to get the size of the CLedController object. The one place
// that includes this just uses extern to declare the function.
// Declaration moved to src/fl/fastled.h
