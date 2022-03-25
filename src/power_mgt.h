#ifndef POWER_MGT_H
#define POWER_MGT_H

#include "FastLED.h"

#include "pixeltypes.h"

/// @file power_mgt.h
/// Functions to limit the power used by FastLED

FASTLED_NAMESPACE_BEGIN

/// @defgroup Power Power Management Functions
/// Functions to limit the amount of power used by FastLED
/// @{


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
uint8_t  calculate_max_brightness_for_power_mW( uint8_t target_brightness, uint32_t max_power_mW);

/// @} PowerInternal


/// @} Power

FASTLED_NAMESPACE_END

#endif
