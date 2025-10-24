#pragma once

/// @file attiny_detect.h
/// Automatic detection of ATtiny platforms
/// This header auto-defines LIB8_ATTINY based on compiler-defined macros
/// It must be included BEFORE any conditional compilation checks for LIB8_ATTINY

#if !defined(LIB8_ATTINY)
#if defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny13A__) || \
    defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
    defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || \
    defined(__AVR_ATtiny48__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny88__) || \
    defined(__AVR_ATtiny167__) || \
    defined(__AVR_ATtiny261__) || defined(__AVR_ATtiny441__) || defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny861__) || \
    defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__) || defined(__AVR_ATtiny4313__)
#define LIB8_ATTINY 1
#endif
#endif

// Define LIB8_ATTINY_NO_UART for ATtiny chips without UART hardware (only USI)
#if !defined(LIB8_ATTINY_NO_UART)
#if defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny13A__) || \
    defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
    defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || \
    defined(__AVR_ATtiny88__)
#define LIB8_ATTINY_NO_UART 1
#endif
#endif
