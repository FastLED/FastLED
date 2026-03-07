// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTLED_AVR_H
#define __INC_FASTLED_AVR_H

#include "platforms/avr/is_avr.h"
#include "platforms/avr/fastpin_avr.h"
#include "platforms/avr/fastspi_avr.h"

// Include appropriate clockless controller based on platform
#ifdef FL_IS_AVR_ATTINY
  // ATtiny platforms use optimized assembly implementation
  #include "platforms/avr/attiny/clockless_blocking.h"
#else
  // Other AVR platforms use standard clockless controller
  #include "platforms/avr/clockless_avr.h"
#endif

// Default to using PROGMEM
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif

#endif
