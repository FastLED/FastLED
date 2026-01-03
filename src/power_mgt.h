#pragma once

#include "fl/fastled.h"

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
struct PowerModelRGB {
    uint8_t red_mW;    ///< Red channel power at full brightness (255), in milliwatts
    uint8_t green_mW;  ///< Green channel power at full brightness (255), in milliwatts
    uint8_t blue_mW;   ///< Blue channel power at full brightness (255), in milliwatts
    uint8_t dark_mW;   ///< Dark LED baseline power consumption, in milliwatts

    /// Default constructor - WS2812 @ 5V (16mA/11mA/15mA @ 5V)
    constexpr PowerModelRGB()
        : red_mW(5 * 16), green_mW(5 * 11), blue_mW(5 * 15), dark_mW(5 * 1) {}

    /// Custom RGB power model
    /// @param r Red channel power (mW)
    /// @param g Green channel power (mW)
    /// @param b Blue channel power (mW)
    /// @param d Dark state power (mW)
    constexpr PowerModelRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t d)
        : red_mW(r), green_mW(g), blue_mW(b), dark_mW(d) {}
};

/// RGBW LED power consumption model
/// @note Future API enhancement - not yet implemented in power calculations
/// @note Currently forwards to PowerModelRGB, ignoring white channel
struct PowerModelRGBW {
    uint8_t red_mW;    ///< Red channel power at full brightness (255), in milliwatts
    uint8_t green_mW;  ///< Green channel power at full brightness (255), in milliwatts
    uint8_t blue_mW;   ///< Blue channel power at full brightness (255), in milliwatts
    uint8_t white_mW;  ///< White channel power at full brightness (255), in milliwatts
    uint8_t dark_mW;   ///< Dark LED baseline power consumption, in milliwatts

    /// Default constructor - SK6812 RGBW @ 5V estimate
    constexpr PowerModelRGBW()
        : red_mW(90), green_mW(70), blue_mW(90), white_mW(100), dark_mW(5) {}

    /// Custom RGBW power model
    /// @param r Red channel power (mW)
    /// @param g Green channel power (mW)
    /// @param b Blue channel power (mW)
    /// @param w White channel power (mW)
    /// @param d Dark state power (mW)
    constexpr PowerModelRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t d)
        : red_mW(r), green_mW(g), blue_mW(b), white_mW(w), dark_mW(d) {}

    /// Convert to RGB model (extracts RGB components only)
    /// @note Used internally until RGBW power calculations are implemented
    constexpr PowerModelRGB toRGB() const {
        return PowerModelRGB(red_mW, green_mW, blue_mW, dark_mW);
    }
};

/// RGBWW LED power consumption model (RGB + Cool White + Warm White)
/// @note Future API enhancement - not yet implemented in power calculations
/// @note Currently forwards to PowerModelRGB, ignoring white channels
struct PowerModelRGBWW {
    uint8_t red_mW;        ///< Red channel power at full brightness (255), in milliwatts
    uint8_t green_mW;      ///< Green channel power at full brightness (255), in milliwatts
    uint8_t blue_mW;       ///< Blue channel power at full brightness (255), in milliwatts
    uint8_t white_mW;      ///< Cool white channel power at full brightness (255), in milliwatts
    uint8_t warm_white_mW; ///< Warm white channel power at full brightness (255), in milliwatts
    uint8_t dark_mW;       ///< Dark LED baseline power consumption, in milliwatts

    /// Default constructor - Hypothetical RGBWW @ 5V estimate
    constexpr PowerModelRGBWW()
        : red_mW(85), green_mW(65), blue_mW(85),
          white_mW(95), warm_white_mW(95), dark_mW(5) {}

    /// Custom RGBWW power model
    /// @param r Red channel power (mW)
    /// @param g Green channel power (mW)
    /// @param b Blue channel power (mW)
    /// @param w Cool white channel power (mW)
    /// @param ww Warm white channel power (mW)
    /// @param d Dark state power (mW)
    constexpr PowerModelRGBWW(uint8_t r, uint8_t g, uint8_t b,
                              uint8_t w, uint8_t ww, uint8_t d)
        : red_mW(r), green_mW(g), blue_mW(b),
          white_mW(w), warm_white_mW(ww), dark_mW(d) {}

    /// Convert to RGB model (extracts RGB components only)
    /// @note Used internally until RGBWW power calculations are implemented
    constexpr PowerModelRGB toRGB() const {
        return PowerModelRGB(red_mW, green_mW, blue_mW, dark_mW);
    }
};

/// Set custom RGB LED power consumption model
/// @param model RGB power consumption model
void set_power_model(const PowerModelRGB& model);

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
void set_max_power_in_volts_and_milliamps( uint8_t volts, uint32_t milliamps);

/// Set the maximum power used in watts
/// @deprecated Use CFastLED::setMaxPowerInMilliWatts
void set_max_power_in_milliwatts( uint32_t powerInmW);

/// Select a pin with an LED that will be flashed to indicate that power management
/// is pulling down the brightness
/// @param pinNumber output pin. Zero is "no indicator LED".
void set_max_power_indicator_LED( uint8_t pinNumber); // zero = no indicator LED

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
void delay_at_max_brightness_for_power( uint16_t ms);

/// @} PowerShowDelay


/// @name Power Control Internal Helper Functions
/// Internal helper functions for power control.
/// @{

/// Determines how many milliwatts the current LED data would draw
/// at max brightness (255)
/// @param ledbuffer the LED data to check
/// @param numLeds the number of LEDs in the data array
/// @returns the number of milliwatts the LED data would consume at max brightness
uint32_t calculate_unscaled_power_mW( const CRGB* ledbuffer, uint16_t numLeds);

/// Determines the highest brightness level you can use and still stay under
/// the specified power budget for a given set of LEDs.
/// @param ledbuffer the LED data to check
/// @param numLeds the number of LEDs in the data array
/// @param target_brightness the brightness you'd ideally like to use
/// @param max_power_mW the max power draw desired, in milliwatts
/// @returns a limited brightness value. No higher than the target brightness,
/// but may be lower depending on the power limit.
uint8_t calculate_max_brightness_for_power_mW(const CRGB* ledbuffer, uint16_t numLeds, uint8_t target_brightness, uint32_t max_power_mW);

/// @copybrief calculate_max_brightness_for_power_mW()
/// @param ledbuffer the LED data to check
/// @param numLeds the number of LEDs in the data array
/// @param target_brightness the brightness you'd ideally like to use
/// @param max_power_V the max power in volts
/// @param max_power_mA the max power in milliamps
/// @returns a limited brightness value. No higher than the target brightness,
/// but may be lower depending on the power limit.
uint8_t calculate_max_brightness_for_power_vmA(const CRGB* ledbuffer, uint16_t numLeds, uint8_t target_brightness, uint32_t max_power_V, uint32_t max_power_mA);

/// Determines the highest brightness level you can use and still stay under
/// the specified power budget for all sets of LEDs. 
/// Unlike the other internal power functions which use a pointer to a
/// specific set of LED data, this function uses the ::CFastLED linked list
/// of LED controllers and their attached data.
/// @param target_brightness the brightness you'd ideally like to use
/// @param max_power_mW the max power draw desired, in milliwatts
/// @returns a limited brightness value. No higher than the target brightness,
/// but may be lower depending on the power limit.
uint8_t  calculate_max_brightness_for_power_mW( uint8_t target_brightness, fl::u32 max_power_mW);

/// @} PowerInternal


/// @} Power
