#pragma once

#define FASTLED_INTERNAL  
#include "FastLED.h"

namespace fl {
void avr_compile_tests() {
#if FASTLED_USE_PROGMEM != 1
#error "FASTLED_USE_PROGMEM should be 1 for AVR"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 0
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 0 for AVR"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 0
#error "FASTLED_ALLOW_INTERRUPTS should be 0 for AVR (default)"
#endif

#ifndef FASTLED_AVR
#error "FASTLED_AVR should be defined for AVR platforms"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for AVR platforms"
#endif

// AVR should have a reasonable F_CPU (typically 8MHz or 16MHz)
#if F_CPU < 1000000 || F_CPU > 32000000
#warning "AVR F_CPU seems unusual - check if this is expected"
#endif

// Check for Arduino environment on AVR
#ifndef ARDUINO
#warning "ARDUINO macro not defined - may not be in Arduino environment"
#endif

// AVR-specific assembly optimizations should be enabled
#ifndef QADD8_AVRASM
#warning "AVR assembly optimizations may not be enabled"
#endif
}
}  // namespace fl
