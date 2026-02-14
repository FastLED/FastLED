// IWYU pragma: private

// Definition file for clockless_avr.h
// This file contains static variable definitions that cannot be in the header
// due to Teensy 3.0/3.1/3.2 __cxa_guard linkage issues.

#include "platforms/avr/is_avr.h"

// ATtiny platforms use their own optimized implementation in attiny/clockless_blocking.h
// This file should not be compiled for ATtiny to avoid template redefinition conflicts
#if defined(FASTLED_AVR) && !defined(FL_IS_AVR_ATTINY)

#include "clockless_avr.h"

namespace fl {



}  // namespace fl

#endif  // FASTLED_AVR && !FL_IS_AVR_ATTINY
