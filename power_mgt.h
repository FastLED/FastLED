#ifndef POWER_MGT_H
#define POWER_MGT_H

#include "pixeltypes.h"

FASTLED_NAMESPACE_BEGIN

///@defgroup Power Power management functions
/// functions used to limit the amount of power used by FastLED

// Power Control setup functions
//
// Example:
//  set_max_power_in_volts_and_milliamps( 5, 400);
//

/// Set the maximum power used in milliamps for a given voltage
void set_max_power_in_volts_and_milliamps( uint8_t volts, uint32_t milliamps);
/// Set the maximum power used in watts
void set_max_power_in_milliwatts( uint32_t powerInmW);

/// Select a ping with an led that will be flashed to indicate that power management
/// is pulling down the brightness
void set_max_power_indicator_LED( uint8_t pinNumber); // zero = no indicator LED


// Power Control 'show' and 'delay' functions
//
// These are drop-in replacements for FastLED.show() and FastLED.delay()
// In order to use these, you have to actually replace your calls to
// FastLED.show() and FastLED.delay() with these two functions.
//
// Example:
//     // was: FastLED.show();
//     // now is:
//     show_at_max_brightness_for_power();
//

/// Similar to FastLED.show, but pre-adjusts brightness to keep below the power
/// threshold.
void show_at_max_brightness_for_power();
/// Similar to FastLED.delay, but pre-adjusts brightness to keep below the power
/// threshold.
void delay_at_max_brightness_for_power( uint16_t ms);


// Power Control internal helper functions

/// calculate_unscaled_power_mW tells you how many milliwatts the current
///   LED data would draw at brightness = 255.
///
uint32_t calculate_unscaled_power_mW( const CRGB* ledbuffer, uint16_t numLeds);

/// calculate_max_brightness_for_power_mW tells you the highest brightness
///   level you can use and still stay under the specified power budget.  It
///   takes a 'target brightness' which is the brightness you'd ideally like
///   to use.  The result from this function will be no higher than the
///   target_brightess you supply, but may be lower.
uint8_t  calculate_max_brightness_for_power_mW( uint8_t target_brightness, uint32_t max_power_mW);

FASTLED_NAMESPACE_END
///@}
// POWER_MGT_H

#endif
