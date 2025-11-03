// ok no namespace fl
#ifndef __INC_FASTLED_AVR_H
#define __INC_FASTLED_AVR_H

#include "fastpin_avr.h"
#include "fastspi_avr.h"

// Include appropriate clockless controller based on platform
#include "lib8tion/attiny_detect.h"
#if defined(LIB8_ATTINY)
  // ATtiny platforms use optimized assembly implementation
  #include "platforms/avr/attiny/clockless_blocking.h"
#else
  // Other AVR platforms use standard clockless controller
  #include "clockless_trinket.h"
#endif

// Default to using PROGMEM
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif

#endif
