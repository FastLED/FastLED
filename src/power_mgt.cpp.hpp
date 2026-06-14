/// @file power_mgt.cpp
/// Functions to limit the power used by FastLED

/// Disables pragma messages and warnings
#include "led_sysdefs.h"      // Must be included first (required by lib8tion.h)
#include "pixeltypes.h"       // CRGB
#include "controller.h"       // CLEDController
#include "fastpin.h"          // Pin
#include "fl/system/sketch_macros.h"
#if SKETCH_HAS_LARGE_MEMORY
#include "fl/math/math.h"  // fl::pow, fl::lround — libm-gated wrappers
#include "fl/stl/array.h"
#endif
#include "fl/stl/int.h"           // fl::u32, fl::u8
#include "power_mgt.h"        // Function declarations (to avoid redefinition errors)
#include "fl/stl/singleton.h"    // fl::Singleton
// POWER MANAGEMENT

/// @name Power Usage Values
/// These power usage values are approximate, and your exact readings
/// will be slightly (10%?) different from these.
///
/// They were arrived at by actually measuing the power draw of a number
/// of different LED strips, and a bunch of closed-loop-feedback testing
/// to make sure that if we USE these values, we stay at or under
/// the target power consumption.
/// Actual power consumption is much, much more complicated and has
/// to include things like voltage drop, etc., etc.
/// However, this is good enough for most cases, and almost certainly better
/// than no power management at all.
///
/// You can now customize these values using the PowerModel API:
/// @code
/// FastLED.setPowerModel(PowerModelRGB(40, 40, 40, 2)); // WS2812B @ 3.3V
/// @endcode
/// @{

static constexpr float kLinearPowerScalingExponent = 1.0f;
static constexpr float kPowerScalingExponentEpsilon = 0.0001f;

/// Global RGB power model (initialized to WS2812 @ 5V defaults, linear response)
static PowerModelRGB& gPowerModel() {
    return fl::Singleton<PowerModelRGB>::instance();
}

#if SKETCH_HAS_LARGE_MEMORY
static constexpr fl::size kPowerScalingTableSize = 256;

/// Cached forward/reverse LUTs derived from `gPowerModel().exponent`.
/// Authoritative exponent storage lives in the PowerModel itself; this is a
/// pure cache rebuilt whenever the model's exponent changes.
struct PowerScalingState {
    fl::array<fl::u8, kPowerScalingTableSize> forward;
    fl::array<fl::u8, kPowerScalingTableSize> reverse;

    PowerScalingState() {
        reset_identity();
    }

    void reset_identity() {
        for (fl::size i = 0; i < kPowerScalingTableSize; ++i) {
            forward[i] = static_cast<fl::u8>(i);
            reverse[i] = static_cast<fl::u8>(i);
        }
    }
};

static PowerScalingState& gPowerScaling() {
    return fl::Singleton<PowerScalingState>::instance();
}

/// Rebuild the forward/reverse LUTs from the given exponent.
/// Non-positive or near-1.0 exponents collapse to identity tables.
static void rebuild_power_scaling_tables(float exponent) {
    PowerScalingState& state = gPowerScaling();
    if (!(exponent > 0.0f) ||
        fl::almost_equal(exponent, kLinearPowerScalingExponent, kPowerScalingExponentEpsilon)) {
        state.reset_identity();
        return;
    }

    // Forward LUT: source brightness -> scaled brightness via pow(x/255, exponent)
    state.forward[0] = 0;
    for (fl::size i = 1; i < kPowerScalingTableSize; ++i) {
        float normalized = static_cast<float>(i) / 255.0f;
        // Route through fl::powf / fl::lroundf (libm-gated via FL_MATH_USE_LIBM,
        // see fl/math/math.cpp.hpp). Use the float overloads — fl::pow takes
        // double, so passing floats would auto-promote and re-anchor the
        // double-precision soft-FP chain that #3002 is trying to avoid.
        int mapped = static_cast<int>(
            fl::lroundf(fl::powf(normalized, exponent) * 255.0f));
        state.forward[i] = static_cast<fl::u8>(fl::clamp(mapped, 0, 255));
    }
    state.forward[255] = 255;

    // Reverse LUT: scaled brightness -> largest source whose forward value is
    // <= the scaled value. Floor-inverse, so `unmap_power_value()` never
    // rounds a budgeted scaled brightness *up* past the power budget.
    state.reverse[0] = 0;
    int source = 0;
    for (int scaled = 1; scaled < static_cast<int>(kPowerScalingTableSize); ++scaled) {
        while (source < 255 && state.forward[source + 1] <= scaled) {
            ++source;
        }
        state.reverse[scaled] = static_cast<fl::u8>(source);
    }
}
#endif

static fl::u8 map_power_value(fl::u8 brightness) {
#if SKETCH_HAS_LARGE_MEMORY
    return gPowerScaling().forward[brightness];
#else
    return brightness;
#endif
}

static fl::u8 unmap_power_value(fl::u8 scaled_brightness) {
#if SKETCH_HAS_LARGE_MEMORY
    return gPowerScaling().reverse[scaled_brightness];
#else
    return scaled_brightness;
#endif
}

fl::u32 scale_power_for_brightness(fl::u32 total_mW, fl::u8 brightness) {
    return fl::scale32by8(total_mW, map_power_value(brightness));
}

/// @}

// Alternate calibration by RAtkins via pre-PSU wattage measurments;
// these are all probably about 20%-25% too high due to PSU heat losses,
// but if you're measuring wattage on the PSU input side, this may
// be a better set of calibrations.  (WS2812B)
//  static const uint8_t gRed_mW   = 100;
//  static const uint8_t gGreen_mW =  48;
//  static const uint8_t gBlue_mW  = 100;
//  static const uint8_t gDark_mW  =  12;


/// Debug Option: Set to 1 to enable the power limiting LED
/// @see set_max_power_indicator_LED()
#define POWER_LED 1

/// Debug Option: Set to enable Serial debug statements for power limit functions
/// @note If you enable this, you'll need to include the appropriate headers for Serial (e.g., Arduino.h)
#define POWER_DEBUG_PRINT 0


// Power consumed by the MCU
static const fl::u8 gMCU_mW  =  25 * 5; // 25mA @ 5v = 125 mW

static fl::u8  gMaxPowerIndicatorLEDPinNumber = 0; // default = Arduino onboard LED pin.  set to zero to skip this.


// Span-based version (primary implementation)
fl::u32 calculate_unscaled_power_mW(fl::span<const CRGB> leds) {
    fl::u32 red32 = 0, green32 = 0, blue32 = 0;

    // Iterate using span's safe indexing
    for(size_t i = 0; i < leds.size(); i++) {
        red32   += map_power_value(leds[i].r);
        green32 += map_power_value(leds[i].g);
        blue32  += map_power_value(leds[i].b);
    }

    red32   *= gPowerModel().red_mW;
    green32 *= gPowerModel().green_mW;
    blue32  *= gPowerModel().blue_mW;

    red32   >>= 8;
    green32 >>= 8;
    blue32  >>= 8;

    fl::u32 total = red32 + green32 + blue32 + (gPowerModel().dark_mW * leds.size());

    return total;
}

// Pointer-based version (delegates to span version)
fl::u32 calculate_unscaled_power_mW( const CRGB* ledbuffer, fl::u16 numLeds ) //25354
{
    return calculate_unscaled_power_mW(fl::span<const CRGB>(ledbuffer, numLeds));
}


fl::u8 calculate_max_brightness_for_power_vmA(const CRGB* ledbuffer, fl::u16 numLeds, fl::u8 target_brightness, fl::u32 max_power_V, fl::u32 max_power_mA) {
	return calculate_max_brightness_for_power_mW(ledbuffer, numLeds, target_brightness, max_power_V * max_power_mA);
}

fl::u8 calculate_max_brightness_for_power_mW(const CRGB* ledbuffer, fl::u16 numLeds, fl::u8 target_brightness, fl::u32 max_power_mW) {
 	fl::u32 total_mW = calculate_unscaled_power_mW( ledbuffer, numLeds);

	fl::u8 target_brightness_scaled = map_power_value(target_brightness);
	fl::u32 requested_power_mW = scale_power_for_brightness(total_mW, target_brightness);

	fl::u8 recommended_brightness = target_brightness;
	if(requested_power_mW > max_power_mW) { 
        fl::u8 recommended_scaled = (fl::u32)(target_brightness_scaled * (fl::u32)(max_power_mW)) / requested_power_mW;
        recommended_brightness = unmap_power_value(recommended_scaled);
	}

	return recommended_brightness;
}

// sets brightness to
//  - no more than target_brightness
//  - no more than max_mW milliwatts
fl::u8 calculate_max_brightness_for_power_mW( fl::u8 target_brightness, fl::u32 max_power_mW)
{
    fl::u32 total_mW = gMCU_mW;

    CLEDController *pCur = CLEDController::head();
	while(pCur) {
        total_mW += calculate_unscaled_power_mW( pCur->leds(), pCur->size());
		pCur = pCur->next();
	}

#if POWER_DEBUG_PRINT == 1
    Serial.print("power demand at full brightness mW = ");
    Serial.println( total_mW);
#endif

    fl::u8 target_brightness_scaled = map_power_value(target_brightness);
    fl::u32 requested_power_mW = scale_power_for_brightness(total_mW, target_brightness);
#if POWER_DEBUG_PRINT == 1
    if( target_brightness != 255 ) {
        Serial.print("power demand at scaled brightness mW = ");
        Serial.println( requested_power_mW);
    }
    Serial.print("power limit mW = ");
    Serial.println( max_power_mW);
#endif

    if( requested_power_mW < max_power_mW) {
#if POWER_LED > 0
        if( gMaxPowerIndicatorLEDPinNumber ) {
            Pin(gMaxPowerIndicatorLEDPinNumber).lo(); // turn the LED off
        }
#endif
#if POWER_DEBUG_PRINT == 1
        Serial.print("demand is under the limit");
#endif
        return target_brightness;
    }

    fl::u8 recommended_scaled = (fl::u32)(target_brightness_scaled * (fl::u32)(max_power_mW)) / ((fl::u32)(requested_power_mW));
    fl::u8 recommended_brightness = unmap_power_value(recommended_scaled);
#if POWER_DEBUG_PRINT == 1
    Serial.print("recommended brightness # = ");
    Serial.println( recommended_brightness);

    fl::u32 resultant_power_mW = scale_power_for_brightness(total_mW, recommended_brightness);
    Serial.print("resultant power demand mW = ");
    Serial.println( resultant_power_mW);

    Serial.println();
#endif

#if POWER_LED > 0
    if( gMaxPowerIndicatorLEDPinNumber ) {
        Pin(gMaxPowerIndicatorLEDPinNumber).hi(); // turn the LED on
    }
#endif

    return recommended_brightness;
}

void set_max_power_indicator_LED( fl::u8 pinNumber)
{
    gMaxPowerIndicatorLEDPinNumber = pinNumber;
}

void set_power_model(const PowerModelRGB& model) {
    gPowerModel() = model;
#if SKETCH_HAS_LARGE_MEMORY
    rebuild_power_scaling_tables(model.exponent);
#else
    // Small-memory targets ignore non-linear exponent and keep linear behavior;
    // clamp the stored field so get_power_scaling_exponent() reports the truth.
    gPowerModel().exponent = kLinearPowerScalingExponent;
#endif
}

void set_power_scaling_exponent(float exponent) {
    PowerModelRGB model = gPowerModel();
    model.exponent = exponent;
    set_power_model(model);
}

float get_power_scaling_exponent() {
    // Authoritative storage lives in the model; on small-memory builds the
    // field is clamped to 1.0 by set_power_model.
    return gPowerModel().exponent;
}

PowerModelRGB get_power_model() {
    return gPowerModel();
}

// Note: The following deprecated wrapper functions have been moved to FastLED.cpp:
// - set_max_power_in_volts_and_milliamps()
// - set_max_power_in_milliwatts()
// - show_at_max_brightness_for_power()
// - delay_at_max_brightness_for_power()
// These functions depend on the FastLED singleton and don't belong in this core power calculation file.
