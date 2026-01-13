// Definition file for clockless_avr.h
// This file contains static variable definitions that cannot be in the header
// due to Teensy 3.0/3.1/3.2 __cxa_guard linkage issues.

#include "platforms/avr/is_avr.h"

#ifdef FASTLED_AVR

#include "clockless_avr.h"

namespace fl {



}  // namespace fl

#endif  // FASTLED_AVR
