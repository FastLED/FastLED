#pragma once

// if attiny85 or attiny88, use less leds so this example can compile.
#if defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny88__)
#define HAS_ENOUGH_MEMORY 0
#else
#define HAS_ENOUGH_MEMORY 1
#endif
