#pragma once

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

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328PB__) || \
    defined(__AVR_ATmega328__) || defined(__AVR_ATmega168P__) || \
    defined(__AVR_ATmega168__) || defined(__AVR_ATmega8__)
    // ATmega328P family: Arduino UNO, Nano, Pro Mini
    #include "../m328p/fastpin_m328p.h"

#elif defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
    // ATmega2560 family: Arduino MEGA 2560
    #include "../m2560/fastpin_m2560.h"

#elif defined(__AVR_ATmega32U4__)
    // ATmega32U4 family: Leonardo, Pro Micro, Teensy 2.0
    #include "../m32u4/fastpin_m32u4.h"

#elif defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__) || \
      defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny4313__) || \
      defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny48__) || \
      defined(__AVR_ATtiny88__) || defined(__AVR_ATtiny841__) || \
      defined(__AVR_ATtiny441__) || defined(ARDUINO_AVR_DIGISPARK) || \
      defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || \
      defined(__AVR_ATtiny84__) || defined(ARDUINO_AVR_DIGISPARKPRO) || \
      defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__) || \
      defined(IS_BEAN) || \
      defined(__AVR_ATtiny202__) || defined(__AVR_ATtiny204__) || \
      defined(__AVR_ATtiny212__) || defined(__AVR_ATtiny214__) || \
      defined(__AVR_ATtiny402__) || defined(__AVR_ATtiny404__) || \
      defined(__AVR_ATtiny406__) || defined(__AVR_ATtiny407__) || \
      defined(__AVR_ATtiny412__) || defined(__AVR_ATtiny414__) || \
      defined(__AVR_ATtiny416__) || defined(__AVR_ATtiny417__) || \
      defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtinyxy6__) || \
      defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtinyxy2__)
    // All ATtiny variants (classic and modern)
    #include "../../attiny/pins/fastpin_attiny.h"

#else
    // Remaining ATmega variants: ATmega1284P, ATmega644P, AT90USB, etc.
    #include "fastpin_legacy_other.h"

#endif
