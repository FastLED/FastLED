#ifndef POWER_MGT_H
#define POWER_MGT_H

#include "pixeltypes.h"

// Power Control setup functions
//
// Example:
//  set_max_power_in_volts_and_milliamps( 5, 400);
//
void set_max_power_in_volts_and_milliamps( uint8_t volts, uint32_t milliamps);
void set_max_power_in_milliwatts( uint32_t powerInmW);

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
void show_at_max_brightness_for_power();
void delay_at_max_brightness_for_power( uint16_t ms);


// Power Control internal helper functions
//
// calculate_unscaled_power_mW tells you how many milliwatts the current
//   LED data would draw at brightness = 255.
//
// calculate_max_brightness_for_power_mW tells you the highest brightness
//   level you can use and still stay under the specified power budget.  It
//   takes a 'target brightness' which is the brightness you'd ideally like
//   to use.  The result from this function will be no higher than the
//   target_brightess you supply, but may be lower.
uint32_t calculate_unscaled_power_mW( const CRGB* ledbuffer, uint16_t numLeds);

uint8_t  calculate_max_brightness_for_power_mW( uint8_t target_brightness, uint32_t max_power_mW);


// POWER_MGT_H
#endif
