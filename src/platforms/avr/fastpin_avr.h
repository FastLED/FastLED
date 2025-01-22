#pragma once

// NOTE: If you work on a platform in this file then please split it into it's own file
// like fastpin_avr_nano_every.h here. It turns out that AI is pretty good at finding
// what the correct AVR pin settings but wants to have small files to work with.
// This god-header is too big and AI will stumble trying to generate the correct
// edits.

// Nano Every is also powered by ATmega4809
#ifdef __AVR_ATmega4809__
    #ifdef ARDUINO_AVR_NANO_EVERY
    #include "fastpin_avr_nano_every.h"
    #else
    #include "fastpin_avr_atmega4809.h"
    #endif  // ARDUINO_AVR_NANO_EVERY
#else
#include "fastpin_avr_legacy.h"
#endif // __AVR_ATmega4809__
