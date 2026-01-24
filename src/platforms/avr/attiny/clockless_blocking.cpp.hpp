// Definition file for clockless_blocking.h
// This file contains static variable definitions that cannot be in the header
// due to Teensy 3.0/3.1/3.2 __cxa_guard linkage issues.

#include "platforms/avr/is_avr.h"

#ifdef FL_IS_AVR_ATTINY

#include "clockless_blocking.h"

namespace fl {

#if (!defined(NO_CLOCK_CORRECTION) || (NO_CLOCK_CORRECTION == 0)) && (FASTLED_ALLOW_INTERRUPTS == 0)
uint8_t gTimeErrorAccum256ths = 0;
#endif

}  // namespace fl

#endif  // FL_IS_AVR_ATTINY
