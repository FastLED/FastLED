#pragma once

#include "platforms/avr/is_avr.h"

// Dispatcher for legacy AVR pin mappings (DDR/PORT/PIN architecture)
// Routes to appropriate family-specific implementation based on MCU type
//
// This replaces the old 500-line god-header with a clean routing mechanism.
// Family-specific files are now in:
//   - atmega/m328p/    (Arduino UNO, Nano)
//   - atmega/m32u4/    (Leonardo, Pro Micro, Teensy 2.0)
//   - atmega/m2560/    (Arduino MEGA)
//   - atmega/common/   (Other ATmega variants)
//   - attiny/pins/     (All ATtiny variants)

// Route to appropriate family-specific header based on MCU type
// Note: Family-specific files have their own namespace fl {} wrappers

#ifdef FL_IS_AVR_ATMEGA_328P
    // ATmega328P family: Arduino UNO, Nano, Pro Mini, ATmega8/8A
    #include "platforms/avr/atmega/m328p/fastpin_m328p.h"

#elif defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
    // ATmega2560 family: Arduino MEGA 2560
    #include "platforms/avr/atmega/m2560/fastpin_m2560.h"

#elif defined(__AVR_ATmega32U4__)
    // ATmega32U4 family: Leonardo, Pro Micro, Teensy 2.0
    #include "platforms/avr/atmega/m32u4/fastpin_m32u4.h"

#elif defined(FL_IS_AVR_ATTINY)
    // All ATtiny variants (classic and modern)
    #include "platforms/avr/attiny/pins/fastpin_attiny.h"

#else
    // Remaining ATmega variants: ATmega1284P, ATmega644P, AT90USB, etc.
    #include "fastpin_legacy_other.h"

#endif
