#pragma once

#ifndef __INC_CHIPSETS_H
#define __INC_CHIPSETS_H

// allow-include-after-namespace

#include "pixeltypes.h"
#include "fl/five_bit_hd_gamma.h"
#include "fl/force_inline.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/unique_ptr.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"

// Include centralized LED chipset timing definitions
// These provide unified nanosecond-based T1, T2, T3 timing for all supported chipsets
#include "fl/chipsets/led_timing.h"

// Include legacy AVR-specific timing definitions (FMUL-based)
// Used for backward compatibility with existing AVR clockless drivers
#ifdef __AVR__
#include "platforms/avr/led_timing_legacy_avr.h"
#endif

// Platform-specific clockless controller dispatch
// Includes the appropriate clockless controller implementation for the target platform
#include "platforms/clockless.h"

// Include UCS7604 controller (works on all platforms with clockless controller support)
#include "fl/chipsets/ucs7604.h"
#include "fl/chipsets/encoders/ws2816.h"

// Include platform-independent SPI utilities
#include "platforms/shared/spi_pixel_writer.h"

// Platform-specific SPI output template dispatch
// Includes the appropriate SPIOutput template implementation for the target platform
// This must be included before any SPI chipset controllers (e.g., APA102, P9813)
#include "platforms/spi_output_template.h"

// Include SPI chipset controllers
#include "fl/chipsets/apa102.h"
#include "fl/chipsets/hd108.h"
#include "fl/chipsets/lpd880x.h"
#include "fl/chipsets/ws2801.h"
#include "fl/chipsets/p9813.h"
#include "fl/chipsets/sm16716.h"

// Platform-specific clockless timing configuration
//
// IMPORTANT: All platforms use nanosecond-based timing at the API level.
// Chipset timing values (T1, T2, T3) are specified in nanoseconds (e.g., T1=250ns, T2=625ns, T3=375ns)
//
// Platforms that need cycle-based timing internally (e.g., AVR, ARM, ESP8266) perform
// nanosecond-to-cycle conversion within their clockless driver implementation using F_CPU or equivalent.
//
// Platform-specific clockless drivers are defined in:
// - ESP32/ESP8266: platforms/esp/clockless.h
// - All ARM platforms: led_sysdefs_arm_*.h + platform-specific clockless headers
// - AVR platforms: led_sysdefs_avr.h + platforms/avr/clockless_*.h
// - Other platforms: respective led_sysdefs_*.h files

// ============================================================================
// PLATFORM-SPECIFIC SPECIALIZED CONTROLLERS (Bottom Includes)
// ============================================================================
// The includes below MUST be at this point in the file because:
//
// 1. C++ templates require full definition in headers (no .cpp separation)
// 2. Base templates and type definitions (above) must be defined BEFORE
//    platform-specific specializations (below) can see and use them
// 3. Platform specializations need to instantiate/specialize the base templates
//
// This is intentional C++ template architecture, not a code smell or circular
// dependency. Template-heavy libraries like FastLED naturally have includes at
// the bottom to support platform-specific specializations.
//
// Example: WS2812 is heavily optimized per-platform because it's the most
// popular chipset. Each platform provides specialized implementations that
// fully replace the default template instantiation.
// ============================================================================

#include "platforms/chipsets_specialized_ws2812.h"

#if defined(ARDUINO) && (defined(SoftwareSerial_h) || defined(__SoftwareSerial_h))
#include <SoftwareSerial.h>
#endif

/// @file chipsets.h
/// Contains the bulk of the definitions for the various LED chipsets supported.



/// @defgroup Chipsets LED Chipset Controllers
/// Implementations of ::CLEDController classes for various led chipsets.
///
/// @{

#if defined(ARDUINO) && (defined(SoftwareSerial_h) || defined(__SoftwareSerial_h))

#define HAS_PIXIE


/// Adafruit Pixie controller class
/// @tparam DATA_PIN the pin to write data out on
/// @tparam RGB_ORDER the RGB ordering for the LED data
template<fl::u8 DATA_PIN, EOrder RGB_ORDER = RGB>
class PixieController : public CPixelLEDController<RGB_ORDER> {
	SoftwareSerial Serial;
	CMinWait<2000> mWait;

public:
	PixieController() : Serial(-1, DATA_PIN) {}

protected:
	/// Initialize the controller
	virtual void init() {
		Serial.begin(115200);
		mWait.mark();
	}

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWait.wait();
		while(pixels.has(1)) {
			fl::u8 r = pixels.loadAndScale0();
			Serial.write(r);
			fl::u8 g = pixels.loadAndScale1();
			Serial.write(g);
			fl::u8 b = pixels.loadAndScale2();
			Serial.write(b);
			pixels.advanceData();
			pixels.stepDithering();
		}
		mWait.mark();
	}

};

// template<SoftwareSerial & STREAM, EOrder RGB_ORDER = RGB>
// class PixieController : public PixieBaseController<STREAM, RGB_ORDER> {
// public:
// 	virtual void init() {
// 		STREAM.begin(115200);
// 	}
// };

#endif
#endif

/// @brief Emulation layer to support RGBW LEDs on RGB controllers
/// @details This template class allows you to use RGBW (4-channel) LED strips with
/// controllers that only support RGB (3-channel) output. It works by:
/// 1. Creating an internal buffer to store the converted RGBW data
/// 2. Converting RGB color values to RGBW using configurable color conversion modes
/// 3. Packing the RGBW data (4 bytes per pixel) into RGB format (3 bytes) for transmission
/// 4. Sending the packed data to the underlying RGB controller
///
/// @tparam CONTROLLER The base RGB controller type (e.g., WS2812)
/// @tparam RGB_ORDER The color channel ordering (e.g., GRB for WS2812)
///
/// Usage Example:
/// @code
/// // Define your base RGB controller (must use RGB ordering, no reordering allowed)
/// typedef WS2812<DATA_PIN, RGB> ControllerT;
///
/// // Create the RGBW emulator with your desired color ordering
/// static RGBWEmulatedController<ControllerT, GRB> rgbwController;
///
/// // Add to FastLED
/// FastLED.addLeds(&rgbwController, leds, NUM_LEDS);
/// @endcode
///
/// Color Conversion Modes (via Rgbw parameter):
/// - kRGBWExactColors: Preserves color accuracy, reduces max brightness
/// - kRGBWMaxBrightness: Maximizes brightness, may oversaturate colors
/// - kRGBWBoostedWhite: Boosts white channel for better whites
/// - kRGBWNullWhitePixel: Disables white channel (RGB mode only)
///
/// @note The base CONTROLLER must use RGB ordering (no internal reordering).
/// Color channel reordering is handled by this wrapper class via RGB_ORDER.


template <
	typename CONTROLLER,
	EOrder RGB_ORDER = GRB>  // Default on WS2812>
class RGBWEmulatedController
    : public CPixelLEDController<RGB_ORDER, CONTROLLER::LANES_VALUE,
                                 CONTROLLER::MASK_VALUE> {
  public:
    // ControllerT is a helper class.  It subclasses the device controller class
    // and has three methods to call the three protected methods we use.
    // This is janky, but redeclaring public methods protected in a derived class
    // is janky, too.

    // N.B., byte order must be RGB.
	typedef CONTROLLER ControllerBaseT;
    class ControllerT : public CONTROLLER {
        friend class RGBWEmulatedController<CONTROLLER, RGB_ORDER>;
        void *callBeginShowLeds(int size) { return ControllerBaseT::beginShowLeds(size); }
        void callShow(CRGB *data, int nLeds, fl::u8 brightness) {
            ControllerBaseT::show(data, nLeds, brightness);
        }
        void callEndShowLeds(void *data) { ControllerBaseT::endShowLeds(data); }
    };


    static const int LANES = CONTROLLER::LANES_VALUE;
    static const uint32_t MASK = CONTROLLER::MASK_VALUE;

    // The delegated controller must do no reordering.
    static_assert(RGB == CONTROLLER::RGB_ORDER_VALUE, "The delegated controller MUST NOT do reordering");

    /// @brief Constructor with optional RGBW configuration
    /// @param rgbw Configuration for RGBW color conversion (defaults to kRGBWExactColors mode)
    RGBWEmulatedController(const Rgbw& rgbw = RgbwDefault()) {
        this->setRgbw(rgbw);
    };

    /// @brief Destructor - cleans up the internal RGBW buffer
    ~RGBWEmulatedController() { delete[] mRGBWPixels; }

	virtual void *beginShowLeds(int size) override {
		return mController.callBeginShowLeds(Rgbw::size_as_rgb(size));
	}

	virtual void endShowLeds(void *data) override {
		return mController.callEndShowLeds(data);
	}

    /// @brief Main rendering function that converts RGB to RGBW and shows pixels
    /// @details This function:
    /// 1. Converts each RGB pixel to RGBW format based on the configured conversion mode
    /// 2. Packs the RGBW data into a format the RGB controller can transmit
    /// 3. Temporarily bypasses color correction/temperature on the base controller
    /// 4. Sends the packed data to the physical LED strip
    /// @param pixels The pixel controller containing RGB data to be converted
    virtual void showPixels(PixelController<RGB_ORDER, LANES, MASK> &pixels) override {
        // Ensure buffer is large enough
        ensureBuffer(pixels.size());
		Rgbw rgbw = this->getRgbw();
        fl::u8 *data = fl::bit_cast_ptr<fl::u8>(mRGBWPixels);
        while (pixels.has(1)) {
            pixels.stepDithering();
            pixels.loadAndScaleRGBW(rgbw, data, data + 1, data + 2, data + 3);
            data += 4;
            pixels.advanceData();
        }

		// Force the device controller to a state where it passes data through
		// unmodified: color correction, color temperature, dither, and brightness
		// (passed as an argument to show()).  Temporarily enable the controller,
		// show the LEDs, and disable it again.
		//
		// The device controller is in the global controller list, so if we 
		// don't keep it disabled, it will refresh again with unknown brightness,
		// temperature, etc.

		mController.setCorrection(CRGB(255, 255, 255));
		mController.setTemperature(CRGB(255, 255, 255));
		mController.setDither(DISABLE_DITHER);

		mController.setEnabled(true);
		mController.callShow(mRGBWPixels, Rgbw::size_as_rgb(pixels.size()), 255);
		mController.setEnabled(false);
    }

  private:
    /// @brief Initialize the controller and disable the base controller
    /// @details The base controller is kept disabled to prevent it from
    /// refreshing with its own settings. We only enable it temporarily during show().
    void init() override {
		mController.init();
		mController.setEnabled(false);
	}

    /// @brief Ensures the internal RGBW buffer is large enough for the LED count
    /// @param num_leds Number of RGB LEDs to convert to RGBW
    /// @details Reallocates the buffer if needed, accounting for the 4:3 byte ratio
    /// when packing RGBW data into RGB format
    void ensureBuffer(int32_t num_leds) {
        if (num_leds != mNumRGBLeds) {
            mNumRGBLeds = num_leds;
            // The delegate controller expects the raw pixel byte data in multiples of 3.
            // In the case of src data not a multiple of 3, then we need to
            // add pad bytes so that the delegate controller doesn't walk off the end
            // of the array and invoke a buffer overflow panic.
            uint32_t new_size = Rgbw::size_as_rgb(num_leds);
            delete[] mRGBWPixels;
            mRGBWPixels = new CRGB[new_size];
			// showPixels may never clear the last two pixels.
			for (uint32_t i = 0; i < new_size; i++) {
				mRGBWPixels[i] = CRGB(0, 0, 0);
			}

			mController.setLeds(mRGBWPixels, new_size);
        }
    }

    CRGB *mRGBWPixels = nullptr;        ///< Internal buffer for packed RGBW data
    int32_t mNumRGBLeds = 0;            ///< Number of RGB LEDs in the original array
    int32_t mNumRGBWLeds = 0;           ///< Number of RGBW pixels the buffer can hold
    ControllerT mController;             ///< The underlying RGB controller instance
};

/// @defgroup ClockedChipsets Clocked Chipsets
/// Nominally SPI based, these chipsets have a data and a clock line.
/// @{

/// Note: SPI chipset controllers have been moved to separate headers in src/fl/chipsets/
/// - APA102 family: apa102.h (APA102, APA102HD, SK9822, SK9822HD, HD107, HD107HD)
/// - HD108: hd108.h
/// - LPD880x family: lpd880x.h (LPD8806, LPD6803)
/// - WS2801 family: ws2801.h (WS2801, WS2803)
/// - P9813: p9813.h
/// - SM16716: sm16716.h



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations - see clockless.h for how the timing values are used
//

#ifdef FL_CLOCKLESS_CONTROLLER_DEFINED
/// @defgroup ClocklessChipsets Clockless Chipsets
/// These chipsets have only a single data line.
///
/// The clockless chipset controllers use the same base class
/// and the same protocol, but with varying timing periods.
///
/// These controllers have 3 control points in their cycle for each bit:
///   @code
///   At T=0        : the line is raised hi to start a bit
///   At T=T1       : the line is dropped low to transmit a zero bit
///   At T=T1+T2    : the line is dropped low to transmit a one bit
///   At T=T1+T2+T3 : the cycle is concluded (next bit can be sent)
///   @endcode
///
/// The units used for T1, T2, and T3 is nanoseconds.
///
/// For 8MHz/16MHz/24MHz frequencies, these values are also guaranteed
/// to be integral multiples of an 8MHz clock (125ns increments).
///
/// @note The base class, ClocklessController, is platform-specific.
/// @note Centralized timing definitions are available in:
///   - fl::chipsets::led_timing.h - Nanosecond-based T1, T2, T3 definitions for all chipsets
///   - fl::platforms::avr::led_timing_legacy_avr.h - Legacy AVR FMUL-based definitions (backward compat)
/// @{

// Allow clock that clockless controller is based on to have different
// frequency than the CPU.
#if !defined(CLOCKLESS_FREQUENCY)
    #define CLOCKLESS_FREQUENCY F_CPU
#endif

// We want to force all avr's to use the Trinket controller when running at 8Mhz, because even the 328's at 8Mhz
// need the more tightly defined timeframes.
// Suppress -Wsubobject-linkage warning for controller template instantiations.
// In C++11/14, constexpr variables have internal linkage, which causes the compiler
// to warn when they are used in base classes of externally-linked templates. The macro
// FL_INLINE_CONSTEXPR adds explicit 'inline' in C++17+ to resolve this, but for C++11
// compatibility we suppress the warning here.
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_SUBOBJECT_LINKAGE




// Legacy macro for nanoseconds to clock cycles conversion
// This is no longer used by the core API but kept for backward compatibility
#define FL_NANOSECONDS_TO_CLOCK_CYCLES(_NS) (((_NS * ((CLOCKLESS_FREQUENCY / 1000000L)) + 999)) / 1000)


//
// Given datasheet timing values T0H, T0L, T1H, T1L (in nanoseconds):
//
//  duration = max(T0H + T0L, T1H + T1L)  # Maximum cycle time
//  T1 = T0H                               # High time for '0' bit
//  T2 = T1H - T0H                         # Additional time for '1' bit
//  T3 = duration - T1H                    # Tail time after '1' bit
//
// Example (WS2812B with T0H=400, T0L=850, T1H=850, T1L=400):
//  duration = 1250, T1 = 400, T2 = 450, T3 = 400
//
// For interactive conversion or validation, use: uv run ci/tools/led_timing_conversions.py
// For automated testing of all chipsets: uv run ci/test_led_timing_conversion.py
// For C++ check out
//   * converter: src/fl/clockless/timing_conversion.h
//   * tests: tests/fl/clockless/timing_conversion.cpp


/// GE8822 controller @ 800 kHz - references centralized timing from fl::TIMING_GE8822_800KHZ
/// @see fl::TIMING_GE8822_800KHZ in fl::chipsets::led_timing.h (350, 660, 350 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class GE8822Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_GE8822_800KHZ, RGB_ORDER, 4, false> {};

/// GW6205 controller @ 400 kHz - references centralized timing from fl::TIMING_GW6205_400KHZ
/// @see fl::TIMING_GW6205_400KHZ in fl::chipsets::led_timing.h (800, 800, 800 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller400Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_GW6205_400KHZ, RGB_ORDER, 4, false> {};

/// GW6205 controller @ 800 kHz - references centralized timing from fl::TIMING_GW6205_800KHZ
/// @see fl::TIMING_GW6205_800KHZ in fl::chipsets::led_timing.h (400, 400, 400 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_GW6205_800KHZ, RGB_ORDER> {};

/// UCS1903 controller @ 400 kHz - references centralized timing from fl::TIMING_UCS1903_400KHZ
/// @see fl::TIMING_UCS1903_400KHZ in fl::chipsets::led_timing.h (500, 1500, 500 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_UCS1903_400KHZ, RGB_ORDER> {};

/// UCS1903B controller @ 800 kHz - references centralized timing from fl::TIMING_UCS1903B_800KHZ
/// @see fl::TIMING_UCS1903B_800KHZ in fl::chipsets::led_timing.h (400, 450, 450 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_UCS1903B_800KHZ, RGB_ORDER> {};

/// UCS1904 controller @ 800 kHz - references centralized timing from fl::TIMING_UCS1904_800KHZ
/// @see fl::TIMING_UCS1904_800KHZ in fl::chipsets::led_timing.h (400, 400, 450 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_UCS1904_800KHZ, RGB_ORDER> {};

/// UCS2903 controller - references centralized timing from fl::TIMING_UCS2903
/// @see fl::TIMING_UCS2903 in fl::chipsets::led_timing.h (250, 750, 250 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS2903Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_UCS2903, RGB_ORDER> {};

// Template wrapper to adapt ClocklessControllerImpl (6-param template alias)
// to the 3-param template template parameter expected by UCS7604ControllerT
namespace {
    template<int DATA_PIN, typename TIMING, EOrder RGB_ORDER>
    class ClocklessControllerWrapper : public fl::ClocklessControllerImpl<DATA_PIN, TIMING, RGB_ORDER> {};
}

/// UCS7604 8-bit @ 800 kHz controller - references centralized timing from fl::TIMING_UCS7604_800KHZ
/// @see fl::TIMING_UCS7604_800KHZ in fl::chipsets::led_timing.h (250, 625, 375 ns)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
using UCS7604Controller8bit = fl::UCS7604Controller8bitT<DATA_PIN, RGB_ORDER, ClocklessControllerWrapper>;

/// UCS7604 16-bit @ 800 kHz controller - references centralized timing from fl::TIMING_UCS7604_800KHZ
/// @see fl::TIMING_UCS7604_800KHZ in fl::chipsets::led_timing.h (250, 625, 375 ns)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
using UCS7604Controller16bit = fl::UCS7604Controller16bitT<DATA_PIN, RGB_ORDER, ClocklessControllerWrapper>;

/// UCS7604 16-bit @ 1600 kHz controller - references centralized timing from fl::TIMING_UCS7604_1600KHZ
/// @see fl::TIMING_UCS7604_1600KHZ in fl::chipsets::led_timing.h (100, 350, 175 ns)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
using UCS7604Controller16bit1600 = fl::UCS7604Controller16bit1600T<DATA_PIN, RGB_ORDER, ClocklessControllerWrapper>;

/// TM1809 controller @ 800 kHz - references centralized timing from fl::TIMING_TM1809_800KHZ
/// @see fl::TIMING_TM1809_800KHZ in fl::chipsets::led_timing.h (350, 350, 450 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_TM1809_800KHZ, RGB_ORDER> {};

/// WS2811 controller @ 800kHz (fast mode)
/// @see fl::TIMING_WS2811_800KHZ_LEGACY in fl/chipsets/led_timing.h (250, 350, 650 ns = 1250ns cycle = 800kHz)
/// @note WS2811 supports both 400kHz and 800kHz modes (pins 7&8 configure speed)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2811Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2811_800KHZ_LEGACY, RGB_ORDER> {};

/// WS2813 controller - references centralized timing from fl::TIMING_WS2813
/// @see fl::TIMING_WS2813 in fl::chipsets::led_timing.h (320, 320, 640 ns)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2813Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2813, RGB_ORDER> {};

/// WS2812 controller @ 800 kHz - references centralized timing from fl::TIMING_WS2812_800KHZ
/// @note Timing: 250ns, 625ns, 375ns (overclockable via FASTLED_OVERCLOCK_WS2812)
/// @see fl::TIMING_WS2812_800KHZ in fl::chipsets::led_timing.h (250, 625, 375 ns)
/// @note Timings can be overridden at compile time using FASTLED_WS2812_T1, FASTLED_WS2812_T2, FASTLED_WS2812_T3
#if !FASTLED_WS2812_HAS_SPECIAL_DRIVER
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2812_800KHZ, RGB_ORDER> {};
#endif

/// WS2812B-Mini-V3 controller @ 800 kHz - references centralized timing from fl::TIMING_WS2812B_MINI_V3
/// @note Timing: 220ns, 360ns, 580ns (tighter timing specifications)
/// @see fl::TIMING_WS2812B_MINI_V3 in fl::chipsets::led_timing.h (220, 360, 580 ns)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812BMiniV3Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2812B_MINI_V3, RGB_ORDER> {};

/// WS2812B-V5 controller @ 800 kHz - uses identical timing to Mini-V3
/// @note Timing: 220ns, 360ns, 580ns (V5 and Mini-V3 share the same timing)
/// @see fl::TIMING_WS2812B_V5 in fl::chipsets::led_timing.h (alias to TIMING_WS2812B_MINI_V3)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812BV5Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2812B_V5, RGB_ORDER> {};

/// WS2811 controller @ 400 kHz (slow mode, datasheet standard)
/// @see fl::TIMING_WS2811_400KHZ in fl/chipsets/led_timing.h (800, 800, 900 ns = 2500ns cycle = 400kHz)
/// @note This is the conservative/standard WS2811 mode. For 800kHz mode, use WS2811Controller800Khz.
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2811Controller400Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2811_400KHZ, RGB_ORDER> {};

/// WS2815 controller - references centralized timing from fl::TIMING_WS2815
/// @see fl::TIMING_WS2815 in chipsets::led_timing.h (250, 1090, 550 ns)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2815Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_WS2815, RGB_ORDER> {};

/// DP1903 controller @ 800 kHz - references centralized timing from fl::TIMING_DP1903_800KHZ
/// @see fl::TIMING_DP1903_800KHZ in chipsets::led_timing.h
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class DP1903Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_DP1903_800KHZ, RGB_ORDER> {};

/// DP1903 controller @ 400 kHz - references centralized timing from fl::TIMING_DP1903_400KHZ
/// @see fl::TIMING_DP1903_400KHZ in chipsets::led_timing.h
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class DP1903Controller400Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_DP1903_400KHZ, RGB_ORDER> {};

/// TM1803 controller @ 400 kHz - references centralized timing from fl::TIMING_TM1803_400KHZ
/// @see fl::TIMING_TM1803_400KHZ in chipsets::led_timing.h (700, 1100, 700 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_TM1803_400KHZ, RGB_ORDER> {};

/// TM1829 controller @ 800 kHz - references centralized timing from fl::TIMING_TM1829_800KHZ
/// @see fl::TIMING_TM1829_800KHZ in chipsets::led_timing.h (340, 340, 550 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller800Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_TM1829_800KHZ, RGB_ORDER> {};

/// TM1829 controller @ 1600 kHz - references centralized timing from fl::TIMING_TM1829_1600KHZ
/// @see fl::TIMING_TM1829_1600KHZ in chipsets::led_timing.h (100, 300, 200 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller1600Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_TM1829_1600KHZ, RGB_ORDER> {};

/// LPD1886 controller @ 1250 kHz - references centralized timing from fl::TIMING_LPD1886_1250KHZ
/// @see fl::TIMING_LPD1886_1250KHZ in chipsets::led_timing.h (200, 400, 200 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_LPD1886_1250KHZ, RGB_ORDER> {};

/// LPD1886 controller @ 1250 kHz (8-bit) - references centralized timing from fl::TIMING_LPD1886_1250KHZ
/// @see fl::TIMING_LPD1886_1250KHZ in chipsets::led_timing.h (200, 400, 200 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz_8bit : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_LPD1886_1250KHZ, RGB_ORDER> {};


/// SK6822 controller - references centralized timing from fl::TIMING_SK6822
/// @see fl::TIMING_SK6822 in chipsets::led_timing.h (375, 1000, 375 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6822Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_SK6822, RGB_ORDER> {};

/// SK6812 controller - references centralized timing from fl::TIMING_SK6812
/// @see fl::TIMING_SK6812 in chipsets::led_timing.h (300, 600, 300 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6812Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_SK6812, RGB_ORDER> {};

/// SM16703 controller - references centralized timing from fl::TIMING_SM16703
/// @see fl::TIMING_SM16703 in chipsets::led_timing.h (300, 600, 300 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class SM16703Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_SM16703, RGB_ORDER> {};

/// PL9823 controller - references centralized timing from fl::TIMING_PL9823
/// @see fl::TIMING_PL9823 in chipsets::led_timing.h (350, 1010, 350 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class PL9823Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_PL9823, RGB_ORDER> {};

/// UCS1912 controller - references centralized timing from fl::TIMING_UCS1912
/// @note Never been tested, this is according to the datasheet
/// @see fl::TIMING_UCS1912 in chipsets::led_timing.h (250, 1000, 350 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1912Controller : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_UCS1912, RGB_ORDER> {};

/// SM16824E controller - references centralized timing from fl::TIMING_SM16824E
/// @note NEW LED! Help us test it! Under development.
/// Timing: T0H=.3μs, T0L=.9μs, T1H=.9μs, T1L=.3μs, TRST=200μs
/// @see fl::TIMING_SM16824E in chipsets::led_timing.h (300, 900, 100 ns)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class SM16824EController : public fl::ClocklessControllerImpl<DATA_PIN, fl::TIMING_SM16824E, RGB_ORDER> {};

FL_DISABLE_WARNING_POP
/// @} ClocklessChipsets


// WS2816 - is an emulated controller that emits 48 bit pixels by forwarding
// them to a platform specific WS2812 controller.  The WS2812 controller
// has to output twice as many 24 bit pixels.
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2816Controller
    : public CPixelLEDController<RGB_ORDER, 
                                 WS2812Controller800Khz<DATA_PIN, RGB>::LANES_VALUE,
                                 WS2812Controller800Khz<DATA_PIN, RGB>::MASK_VALUE> {

public:

	// ControllerT is a helper class.  It subclasses the device controller class
	// and has three methods to call the three protected methods we use.
	// This is janky, but redeclaring public methods protected in a derived class
	// is janky, too.

    // N.B., byte order must be RGB.
	typedef WS2812Controller800Khz<DATA_PIN, RGB> ControllerBaseT;
	class ControllerT : public ControllerBaseT {
		friend class WS2816Controller<DATA_PIN, RGB_ORDER>;
		void *callBeginShowLeds(int size) { return ControllerBaseT::beginShowLeds(size); }
		void callShow(CRGB *data, int nLeds, fl::u8 brightness) {
			ControllerBaseT::show(data, nLeds, brightness);
		}
		void callEndShowLeds(void *data) { ControllerBaseT::endShowLeds(data); }
	};

    static const int LANES = ControllerT::LANES_VALUE;
    static const uint32_t MASK = ControllerT::MASK_VALUE;

    WS2816Controller() {}
    ~WS2816Controller() {
        mController.setLeds(nullptr, 0);
    }

    virtual void *beginShowLeds(int size) override {
        mController.setEnabled(true);
		void *result = mController.callBeginShowLeds(2 * size);
        mController.setEnabled(false);
        return result;
    }

    virtual void endShowLeds(void *data) override {
        mController.setEnabled(true);
		mController.callEndShowLeds(data);
        mController.setEnabled(false);
    }

    virtual void showPixels(PixelController<RGB_ORDER, LANES, MASK> &pixels) override {
        // Ensure buffer is large enough
        ensureBuffer(pixels.size());

        // Create PixelIterator from PixelController (WS2816 is RGB-only, no W channel)
        fl::PixelIterator pixel_iter = pixels.as_iterator(fl::Rgbw());

        // Create 16-bit RGB iterator range (handles RGB reordering, scaling, brightness)
        auto rgb16_range = fl::makeScaledPixelRangeRGB16(&pixel_iter);

        // Encode 16-bit RGB pixels to dual 8-bit CRGB format
        fl::encodeWS2816(rgb16_range.first, rgb16_range.second, mData.get());

        // Ensure device controller won't modify color values
        mController.setCorrection(CRGB(255, 255, 255));
        mController.setTemperature(CRGB(255, 255, 255));
        mController.setDither(DISABLE_DITHER);

        // Output the data stream
        mController.setEnabled(true);
        mController.callShow(mData.get(), 2 * pixels.size(), 255);
        mController.setEnabled(false);
    }

private:
    void init() override {
        mController.init();
        mController.setEnabled(false);
    }

    void ensureBuffer(int size_8bit) {
        int size_16bit = 2 * size_8bit;
        if (mController.size() != size_16bit) {
            
            auto new_leds = fl::make_unique<CRGB[]>(size_16bit);
			mData = fl::move(new_leds);
            mController.setLeds(mData.get(), size_16bit);
        }
    }

    fl::unique_ptr<CRGB[]> mData;
    ControllerT mController;
};


#endif

/// @defgroup SilabsChipsets Silicon Labs ezWS2812 Controllers
/// Hardware-accelerated LED controllers for Silicon Labs MGM240/MG24 series
///
/// These controllers use Silicon Labs' ezWS2812 drivers to provide optimized
/// WS2812 LED control on MGM240 and MG24 series microcontrollers.
///
/// Available controllers:
/// - EZWS2812_SPI: Uses hardware SPI (must define FASTLED_USES_EZWS2812_SPI)
/// - EZWS2812_GPIO: Uses optimized GPIO timing (always available)
/// @{

#if defined(ARDUINO_ARCH_SILABS)

#include "platforms/arm/mgm240/clockless_ezws2812_gpio.h"

/// Silicon Labs ezWS2812 GPIO controller (always available)
/// @tparam DATA_PIN the pin to write data out on
/// @tparam RGB_ORDER the RGB ordering for these LEDs (typically GRB for WS2812)
///
/// This controller uses optimized GPIO manipulation with frequency-specific timing.
/// Automatically selects 39MHz or 78MHz implementation based on F_CPU.
///
/// Usage:
/// @code
/// FastLED.addLeds<EZWS2812_GPIO, 7, GRB>(leds, NUM_LEDS);
/// @endcode
template<fl::u8 DATA_PIN, EOrder RGB_ORDER = GRB>
using EZWS2812_GPIO = fl::ClocklessController_ezWS2812_GPIO_Auto<DATA_PIN, RGB_ORDER>;

#ifdef FASTLED_USES_EZWS2812_SPI

#include "platforms/arm/mgm240/clockless_ezws2812_spi.h"

/// Silicon Labs ezWS2812 SPI controller (requires FASTLED_USES_EZWS2812_SPI)
/// @tparam RGB_ORDER the RGB ordering for these LEDs (typically GRB for WS2812)
///
/// This controller uses the MGM240/MG24's hardware SPI peripheral to generate
/// precise WS2812 timing signals. Excellent performance but consumes SPI peripheral.
///
/// IMPORTANT: You must define FASTLED_USES_EZWS2812_SPI before including FastLED.h
///
/// Usage:
/// @code
/// #define FASTLED_USES_EZWS2812_SPI
/// #include <FastLED.h>
///
/// void setup() {
///     FastLED.addLeds<EZWS2812_SPI, GRB>(leds, NUM_LEDS);
/// }
/// @endcode
template<EOrder RGB_ORDER = GRB>
using EZWS2812_SPI = fl::ClocklessController_ezWS2812_SPI<RGB_ORDER>;

#endif // FASTLED_USES_EZWS2812_SPI

#endif // ARDUINO_ARCH_SILABS

/// @} SilabsChipsets

/// @} Chipsets
