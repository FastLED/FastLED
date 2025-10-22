#pragma once

/// @file attiny_detect.h
/// Automatic detection of ATtiny platforms
/// This header auto-defines LIB8_ATTINY based on compiler-defined macros
/// It must be included BEFORE any conditional compilation checks for LIB8_ATTINY

#if !defined(LIB8_ATTINY)
#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || \
    defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny88__) || \
    defined(__AVR_ATtiny4313__) || defined(__AVR_ATtiny861__) || \
    defined(__AVR_ATtiny261__)
#define LIB8_ATTINY 1
#endif
#endif
