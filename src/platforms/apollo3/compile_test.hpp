#pragma once

#define FASTLED_INTERNAL  
#include "FastLED.h"

namespace fl {

void apollo3_compile_tests() {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for Apollo3"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 1
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 1 for Apollo3"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1
#error "FASTLED_ALLOW_INTERRUPTS should be 1 for Apollo3"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for Apollo3"
#endif

// Check that Apollo3-specific features are available
#ifndef APOLLO3
#warning "APOLLO3 macro not defined - this may indicate platform detection issues"
#endif
}
}
