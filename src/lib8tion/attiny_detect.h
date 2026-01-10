#pragma once

#include "platforms/avr/is_avr.h"  // ok platform headers

/// @file attiny_detect.h
/// Automatic detection of ATtiny platforms
/// Now uses centralized FL_IS_AVR_ATTINY from platforms/avr/is_avr.h
/// Defines LIB8_ATTINY as an alias for backward compatibility

// Define LIB8_ATTINY as alias to FL_IS_AVR_ATTINY for backward compatibility
#if !defined(LIB8_ATTINY) && defined(FL_IS_AVR_ATTINY)
#define LIB8_ATTINY FL_IS_AVR_ATTINY
#endif

// Define LIB8_ATTINY_NO_UART for ATtiny chips without UART hardware (only USI)
#if !defined(LIB8_ATTINY_NO_UART) && defined(FL_IS_AVR_ATTINY_NO_UART)
#define LIB8_ATTINY_NO_UART FL_IS_AVR_ATTINY_NO_UART
#endif
