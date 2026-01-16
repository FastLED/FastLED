/*
  FastLED — AVR Platform ISR Trampoline Header
  ---------------------------------------------
  Dispatches to AVR variant-specific ISR implementations (ATmega or ATtiny).

  This trampoline uses the FL_IS_AVR_ATMEGA and FL_IS_AVR_ATTINY macros to select
  the appropriate implementation at compile time.

  ATmega chips: Full Timer1 support (16-bit timer)
  ATtiny chips: No Timer1 hardware, falls back to null implementation

  License: MIT (FastLED)
*/

#pragma once

// ok no namespace fl

#include "platforms/is_platform.h"

// Dispatch to AVR variant-specific implementation
#if defined(FL_IS_AVR_ATMEGA)
    // ATmega chips have Timer1 hardware - use full implementation
    #include "platforms/avr/atmega/isr_avr_atmega.hpp"
#elif defined(FL_IS_AVR_ATTINY)
    // ATtiny chips don't have Timer1 - no implementation available
    // FL_ISR_AVR_IMPLEMENTED will not be defined, triggering null fallback in main isr.h
#else
    // Unknown AVR variant - no implementation
    // FL_ISR_AVR_IMPLEMENTED will not be defined, triggering null fallback in main isr.h
#endif

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts) - All AVR variants
// =============================================================================

#ifdef __AVR__

#include <avr/interrupt.h>

namespace fl {

/// Disable interrupts on AVR
inline void interruptsDisable() {
    cli();
}

/// Enable interrupts on AVR
inline void interruptsEnable() {
    sei();
}

} // namespace fl

// Prevent isr_null.hpp from defining these functions (would cause ODR violation)
#define FL_ISR_GLOBAL_INTERRUPTS_DEFINED

#endif // __AVR__
