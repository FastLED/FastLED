#pragma once

#include "fl/system/fastled.h"

#include "pixeltypes.h"

/// @file power_mgt.h
/// Functions to limit the power used by FastLED



/// @defgroup Power Power Management Functions
/// Functions to limit the amount of power used by FastLED
/// @{


/// @defgroup PowerModel LED Power Consumption Models
/// Configurable power consumption models for different LED types
/// @{

/// RGB LED power consumption model
/// Used for standard 3-channel LEDs (WS2812, WS2812B, APA102, etc.)
///
/// The model carries the brightness-to-power response exponent alongside
/// the per-channel draw so the scaling behavior travels with the model and
/// can be set in a single `set_power_model(...)` call.
struct PowerModelRGB {
    fl::u8 red_mW;    ///< Red channel power at full brightness (255), in milliwatts
    fl::u8 green_mW;  ///< Green channel power at full brightness (255), in milliwatts
    fl::u8 blue_mW;   ///< Blue channel power at full brightness (255), in milliwatts
    fl::u8 dark_mW;   ///< Dark LED baseline power consumption, in milliwatts
    float  exponent;  ///< Brightness-to-power response exponent (1.0 = linear)

    /// Default constructor - WS2812 @ 5V (16mA/11mA/15mA @ 5V), linear response
    constexpr PowerModelRGB()
        : red_mW(5 * 16), green_mW(5 * 11), blue_mW(5 * 15), dark_mW(5 * 1),
          exponent(1.0f) {}

    /// Custom RGB power model
    /// @param r Red channel power (mW)
    /// @param g Green channel power (mW)
    /// @param b Blue channel power (mW)
    /// @param d Dark state power (mW)
    /// @param e Brightness-to-power response exponent. 1.0 = linear (default),
    ///          values < 1.0 model higher-than-linear draw at mid brightness,
    ///          values > 1.0 model lower-than-linear draw. Non-positive or
    ///          near-1.0 values fall back to linear. Only honored on large-memory
    ///          targets; ignored where `SKETCH_HAS_LARGE_MEMORY==0`.
    constexpr PowerModelRGB(fl::u8 r, fl::u8 g, fl::u8 b, fl::u8 d, float e = 1.0f)
        : red_mW(r), green_mW(g), blue_mW(b), dark_mW(d), exponent(e) {}
};

/// RGBW LED power consumption model
/// @note Future API enhancement - not yet implemented in power calculations
/// @note Currently forwards to PowerModelRGB, ignoring white channel
struct PowerModelRGBW {
    fl::u8 red_mW;    ///< Red channel power at full brightness (255), in milliwatts
    fl::u8 green_mW;  ///< Green channel power at full brightness (255), in milliwatts
    fl::u8 blue_mW;   ///< Blue channel power at full brightness (255), in milliwatts
    fl::u8 white_mW;  ///< White channel power at full brightness (255), in milliwatts
    fl::u8 dark_mW;   ///< Dark LED baseline power consumption, in milliwatts
    float  exponent;  ///< Brightness-to-power response exponent (1.0 = linear)

    /// Default constructor - SK6812 RGBW @ 5V estimate, linear response
    constexpr PowerModelRGBW()
        : red_mW(90), green_mW(70), blue_mW(90), white_mW(100), dark_mW(5),
          exponent(1.0f) {}

    /// Custom RGBW power model
    /// @param r Red channel power (mW)
    /// @param g Green channel power (mW)
    /// @param b Blue channel power (mW)
    /// @param w White channel power (mW)
    /// @param d Dark state power (mW)
    /// @param e Brightness-to-power response exponent. See PowerModelRGB for details.
    constexpr PowerModelRGBW(fl::u8 r, fl::u8 g, fl::u8 b, fl::u8 w, fl::u8 d,
                             float e = 1.0f)
        : red_mW(r), green_mW(g), blue_mW(b), white_mW(w), dark_mW(d),
          exponent(e) {}

    /// Convert to RGB model (extracts RGB components, preserves exponent)
    /// @note Used internally until RGBW power calculations are implemented
    constexpr PowerModelRGB toRGB() const {
        return PowerModelRGB(red_mW, green_mW, blue_mW, dark_mW, exponent);
    }
};

/// RGBWW LED power consumption model (RGB + Cool White + Warm White)
/// @note Future API enhancement - not yet implemented in power calculations
/// @note Currently forwards to PowerModelRGB, ignoring white channels
struct PowerModelRGBWW {
    fl::u8 red_mW;        ///< Red channel power at full brightness (255), in milliwatts
    fl::u8 green_mW;      ///< Green channel power at full brightness (255), in milliwatts
    fl::u8 blue_mW;       ///< Blue channel power at full brightness (255), in milliwatts
    fl::u8 white_mW;      ///< Cool white channel power at full brightness (255), in milliwatts
    fl::u8 warm_white_mW; ///< Warm white channel power at full brightness (255), in milliwatts
    fl::u8 dark_mW;       ///< Dark LED baseline power consumption, in milliwatts
    float  exponent;      ///< Brightness-to-power response exponent (1.0 = linear)

    /// Default constructor - Hypothetical RGBWW @ 5V estimate, linear response
    constexpr PowerModelRGBWW()
        : red_mW(85), green_mW(65), blue_mW(85),
          white_mW(95), warm_white_mW(95), dark_mW(5),
          exponent(1.0f) {}

    /// Custom RGBWW power model
    /// @param r Red channel power (mW)
    /// @param g Green channel power (mW)
    /// @param b Blue channel power (mW)
    /// @param w Cool white channel power (mW)
    /// @param ww Warm white channel power (mW)
    /// @param d Dark state power (mW)
    /// @param e Brightness-to-power response exponent. See PowerModelRGB for details.
    constexpr PowerModelRGBWW(fl::u8 r, fl::u8 g, fl::u8 b,
                              fl::u8 w, fl::u8 ww, fl::u8 d,
                              float e = 1.0f)
        : red_mW(r), green_mW(g), blue_mW(b),
          white_mW(w), warm_white_mW(ww), dark_mW(d),
          exponent(e) {}

    /// Convert to RGB model (folds W/WW power back into RGB so the brightness
    /// limiter doesn't under-budget).
    ///
    /// Issue #2558 update: the white-channel mW values are no longer dropped.
    /// They're distributed evenly across the three RGB channels, which:
    ///   - matches the brightness limiter's assumption that each RGB byte has
    ///     a fixed mW cost at full drive (the limiter caps total predicted
    ///     mW against a budget),
    ///   - over-estimates rather than under-estimates when only white
    ///     channels are on (safer for power-limited supplies),
    ///   - accepts that a fully RGBWW-aware brightness limiter (per-channel
    ///     accounting using all 5 mW values directly) is the right long-term
    ///     fix — see Phase G in issue #2558.
    constexpr PowerModelRGB toRGB() const {
        // Distribute white-channel mW evenly across R/G/B. Clamp to 255 (u8
        // max) on overflow — a wrapping static_cast<u8> here would make the
        // brightness limiter *less* conservative on the highest-draw configs
        // (where it most needs to be conservative). The worst-case
        // under-budget per channel from the floor division is 2 mW.
        return PowerModelRGB(
            static_cast<fl::u8>(
                (red_mW + (white_mW + warm_white_mW) / 3) > 255
                    ? 255 : (red_mW + (white_mW + warm_white_mW) / 3)),
            static_cast<fl::u8>(
                (green_mW + (white_mW + warm_white_mW) / 3) > 255
                    ? 255 : (green_mW + (white_mW + warm_white_mW) / 3)),
            static_cast<fl::u8>(
                (blue_mW + (white_mW + warm_white_mW) / 3) > 255
                    ? 255 : (blue_mW + (white_mW + warm_white_mW) / 3)),
            dark_mW,
            exponent);
    }
};

/// Set custom RGB LED power consumption model
/// @param model RGB power consumption model. The model's `exponent` field drives
///        the brightness-to-power response curve used by estimation and limiting;
///        pass `PowerModelRGB(r, g, b, d, e)` to set channel weights + response
///        curve in a single call.
/// @note `exponent <= 0` or values within 1e-4 of 1.0 fall back to linear
///       (identity tables) rather than rebuilding with a degenerate curve.
void set_power_model(const PowerModelRGB& model);

/// Set a non-linear brightness-to-power response exponent
/// @param exponent 1.0 = linear (default), values below 1.0 model higher-than-linear
///        power draw at mid brightness, values above 1.0 model lower-than-linear draw
/// @note Convenience wrapper: equivalent to setting `model.exponent` and calling
///       `set_power_model`. Prefer `set_power_model(PowerModelRGB{..., e})` to
///       configure channel weights and response in one step.
/// @note This only affects power estimation and limiting, not rendered LED brightness.
/// @note `exponent <= 0` or values within 1e-4 of 1.0 fall back to linear
///       (identity tables) rather than rebuilding with a degenerate curve.
/// @note Only enabled on platforms where `SKETCH_HAS_LARGE_MEMORY==1`.
///       Smaller-memory targets keep the legacy linear behavior and ignore this setting.
void set_power_scaling_exponent(float exponent);

/// Get the current brightness-to-power response exponent
/// @returns the configured exponent. Defaults to 1.0 for linear scaling.
/// @note Returns 1.0 on platforms where `SKETCH_HAS_LARGE_MEMORY==0`.
float get_power_scaling_exponent();

/// Set custom RGBW LED power consumption model
/// @param model RGBW power consumption model
/// @note Future API enhancement - currently extracts RGB components only
/// @note White channel power is stored but not yet used in calculations
inline void set_power_model(const PowerModelRGBW& model) {
    set_power_model(model.toRGB());  // TODO: Implement RGBW power model.
}

/// Set custom RGBWW LED power consumption model
/// @param model RGBWW power consumption model
/// @note Future API enhancement - currently extracts RGB components only
/// @note White channel power is stored but not yet used in calculations
inline void set_power_model(const PowerModelRGBWW& model) {
    set_power_model(model.toRGB());  // TODO: Implement RGBWW power model.
}

/// Get current RGB power model
/// @returns Current RGB power consumption model
PowerModelRGB get_power_model();

/// @} PowerModel


/// @name Power Control Setup Functions
/// Functions to initialize the power control system
/// @{

/// Set the maximum power used in milliamps for a given voltage
/// @deprecated Use CFastLED::setMaxPowerInVoltsAndMilliamps()
void set_max_power_in_volts_and_milliamps( fl::u8 volts, fl::u32 milliamps);

/// Set the maximum power used in watts
/// @deprecated Use CFastLED::setMaxPowerInMilliWatts
void set_max_power_in_milliwatts( fl::u32 powerInmW);

/// Select a pin with an LED that will be flashed to indicate that power management
/// is pulling down the brightness
/// @param pinNumber output pin. Zero is "no indicator LED".
void set_max_power_indicator_LED( fl::u8 pinNumber); // zero = no indicator LED

/// @} PowerSetup


/// @name Power Control 'show()' and 'delay()' Functions
/// Power-limiting replacements of `show()` and `delay()`. 
/// These are drop-in replacements for CFastLED::show() and CFastLED::delay().
/// In order to use these, you have to actually replace your calls to
/// CFastLED::show() and CFastLED::delay() with these two functions.
/// @deprecated These functions are deprecated as of [6ebcb64](https://github.com/FastLED/FastLED/commit/6ebcb6436273cc9a9dc91733af8dfd1fedde6d60),
/// circa 2015. Do not use them for new programs.
///
/// @{

/// Similar to CFastLED::show(), but pre-adjusts brightness to keep
/// below the power threshold.
/// @deprecated This is now a part of CFastLED::show()
void show_at_max_brightness_for_power();
/// Similar to CFastLED::delay(), but pre-adjusts brightness to keep below the power
/// threshold.
/// @deprecated This is now a part of CFastLED::delay()
void delay_at_max_brightness_for_power( fl::u16 ms);

/// @} PowerShowDelay


/// @name Power Control Internal Helper Functions
/// Internal helper functions for power control.
/// @{

/// Determines how many milliwatts the current LED data would draw
/// at max brightness (255)
/// @param ledbuffer the LED data to check
/// @param numLeds the number of LEDs in the data array
/// @returns the number of milliwatts the LED data would consume at max brightness
fl::u32 calculate_unscaled_power_mW( const CRGB* ledbuffer, fl::u16 numLeds);

/// @copydoc calculate_unscaled_power_mW(const CRGB*, uint16_t)
/// @param leds span of LED data to check
fl::u32 calculate_unscaled_power_mW(fl::span<const CRGB> leds);

/// Applies the configured power-scaling response to a total power value
/// @param total_mW unscaled total power at full brightness
/// @param brightness requested brightness in FastLED's 0-255 brightness space
/// @returns estimated power after applying the configured brightness-to-power response
fl::u32 scale_power_for_brightness(fl::u32 total_mW, fl::u8 brightness);

/// Determines the highest brightness level you can use and still stay under
/// the specified power budget for a given set of LEDs.
/// @param ledbuffer the LED data to check
/// @param numLeds the number of LEDs in the data array
/// @param target_brightness the brightness you'd ideally like to use
/// @param max_power_mW the max power draw desired, in milliwatts
/// @returns a limited brightness value. No higher than the target brightness,
/// but may be lower depending on the power limit.
fl::u8 calculate_max_brightness_for_power_mW(const CRGB* ledbuffer, fl::u16 numLeds, fl::u8 target_brightness, fl::u32 max_power_mW);

/// @copybrief calculate_max_brightness_for_power_mW()
/// @param ledbuffer the LED data to check
/// @param numLeds the number of LEDs in the data array
/// @param target_brightness the brightness you'd ideally like to use
/// @param max_power_V the max power in volts
/// @param max_power_mA the max power in milliamps
/// @returns a limited brightness value. No higher than the target brightness,
/// but may be lower depending on the power limit.
fl::u8 calculate_max_brightness_for_power_vmA(const CRGB* ledbuffer, fl::u16 numLeds, fl::u8 target_brightness, fl::u32 max_power_V, fl::u32 max_power_mA);

/// Determines the highest brightness level you can use and still stay under
/// the specified power budget for all sets of LEDs. 
/// Unlike the other internal power functions which use a pointer to a
/// specific set of LED data, this function uses the ::CFastLED linked list
/// of LED controllers and their attached data.
/// @param target_brightness the brightness you'd ideally like to use
/// @param max_power_mW the max power draw desired, in milliwatts
/// @returns a limited brightness value. No higher than the target brightness,
/// but may be lower depending on the power limit.
fl::u8  calculate_max_brightness_for_power_mW( fl::u8 target_brightness, fl::u32 max_power_mW);

/// @} PowerInternal


/// @} Power
