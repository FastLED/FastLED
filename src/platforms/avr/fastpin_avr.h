// ok no namespace fl
#pragma once

// IWYU pragma: private

// NOTE: If you work on a platform in this file then please split it into it's own file
// like fastpin_avr_nano_every.h here. It turns out that AI is pretty good at finding
// what the correct AVR pin settings but wants to have small files to work with.
// This god-header is too big and AI will stumble trying to generate the correct
// edits.

// Nano Every is also powered by ATmega4809
#ifdef __AVR_ATmega4809__
    #ifdef ARDUINO_AVR_NANO_EVERY
    // IWYU pragma: begin_keep
    #include "atmega/m4809/fastpin_avr_nano_every.h"
    // IWYU pragma: end_keep
    #else
    // IWYU pragma: begin_keep
    #include "atmega/m4809/fastpin_avr_atmega4809.h"
    // IWYU pragma: end_keep
    #endif  // ARDUINO_AVR_NANO_EVERY
#else
    // Legacy DDR/PORT/PIN architecture
    // IWYU pragma: begin_keep
    #include "atmega/common/fastpin_avr_legacy_dispatcher.h"
    // IWYU pragma: end_keep
#endif // __AVR_ATmega4809__
