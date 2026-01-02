#pragma once

// ok no namespace fl

/// @file is_avr.h
/// Centralized AVR platform detection macros
/// Defines FL_IS_AVR, FL_IS_AVR_ATMEGA, and FL_IS_AVR_ATTINY for use throughout the codebase
/// This eliminates the need for repeated long lists of #if defined(__AVR_ATmega...) checks

// ============================================================================
// FL_IS_AVR - General AVR platform detection
// ============================================================================
#if defined(__AVR__)
#define FL_IS_AVR
#endif

// ============================================================================
// FL_IS_AVR_ATMEGA - ATmega family detection (classic Timer1-based chips)
// Includes all classic ATmega variants: ATmega328P, ATmega2560, ATmega32U4, etc.
// EXCLUDES megaAVR 0-series/1-series (e.g., ATmega4809) - they use TCB timers
// ============================================================================
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328PB__) || \
    defined(__AVR_ATmega328__) || defined(__AVR_ATmega168P__) || \
    defined(__AVR_ATmega168__) || defined(__AVR_ATmega8__) || \
    defined(__AVR_ATmega8A__) || \
    defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__) || \
    defined(__AVR_ATmega32U4__) || \
    defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__) || \
    defined(__AVR_ATmega644P__) || defined(__AVR_ATmega644__) || \
    defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__) || \
    defined(__AVR_ATmega128__) || defined(__AVR_ATmega64__) || \
    defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U2__) || \
    defined(__AVR_ATmega8U2__) || \
    defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB646__) || \
    defined(__AVR_AT90USB162__) || defined(__AVR_AT90USB82__) || \
    defined(__AVR_ATmega128RFA1__) || defined(__AVR_ATmega256RFR2__)
#define FL_IS_AVR_ATMEGA
#endif

// ============================================================================
// FL_IS_AVR_ATMEGA_328P - ATmega328P family detection (Arduino UNO/Nano)
// Includes: ATmega328P, ATmega328PB, ATmega328, ATmega168P, ATmega168, ATmega8
// ============================================================================
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328PB__) || \
    defined(__AVR_ATmega328__) || defined(__AVR_ATmega168P__) || \
    defined(__AVR_ATmega168__) || defined(__AVR_ATmega8__) || \
    defined(__AVR_ATmega8A__)
#define FL_IS_AVR_ATMEGA_328P
#endif

// ============================================================================
// FL_IS_AVR_MEGAAVR - megaAVR 0-series/1-series detection
// These are modern AVR chips with different peripherals (TCB timers, no Timer1)
// Includes: ATmega4809 (Nano Every), ATmega4808, ATmega3209, etc.
// These chips are NOT compatible with classic ATmega Timer1-based code
// ============================================================================
#if defined(__AVR_ATmega4809__) || defined(__AVR_ATmega4808__) || \
    defined(__AVR_ATmega3209__) || defined(__AVR_ATmega3208__) || \
    defined(__AVR_ATmega1609__) || defined(__AVR_ATmega1608__) || \
    defined(__AVR_ATmega809__) || defined(__AVR_ATmega808__)
#define FL_IS_AVR_MEGAAVR
#endif

// ============================================================================
// FL_IS_AVR_ATTINY - ATtiny family detection
// Includes both classic ATtiny (85, 84, etc.) and modern tinyAVR (412, 1614, etc.)
// ============================================================================
#if defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny13A__) || \
    defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
    defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || \
    defined(__AVR_ATtiny48__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny88__) || \
    defined(__AVR_ATtiny167__) || \
    defined(__AVR_ATtiny261__) || defined(__AVR_ATtiny441__) || defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny861__) || \
    defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__) || defined(__AVR_ATtiny4313__) || \
    defined(__AVR_ATtinyX41__) || \
    defined(__AVR_ATtiny202__) || defined(__AVR_ATtiny204__) || \
    defined(__AVR_ATtiny212__) || defined(__AVR_ATtiny214__) || \
    defined(__AVR_ATtiny402__) || defined(__AVR_ATtiny404__) || \
    defined(__AVR_ATtiny406__) || defined(__AVR_ATtiny407__) || \
    defined(__AVR_ATtiny412__) || defined(__AVR_ATtiny414__) || \
    defined(__AVR_ATtiny416__) || defined(__AVR_ATtiny417__) || \
    defined(__AVR_ATtiny1604__) || defined(__AVR_ATtiny1616__) || \
    defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtinyxy6__) || \
    defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtinyxy2__) || \
    defined(ARDUINO_AVR_DIGISPARK) || defined(ARDUINO_AVR_DIGISPARKPRO) || \
    defined(IS_BEAN)
#define FL_IS_AVR_ATTINY
#endif

// ============================================================================
// FL_IS_AVR_ATTINY_NO_UART - ATtiny chips without UART hardware (only USI)
// ============================================================================
#if defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny13A__) || \
    defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
    defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || \
    defined(__AVR_ATtiny88__)
#define FL_IS_AVR_ATTINY_NO_UART
#endif
