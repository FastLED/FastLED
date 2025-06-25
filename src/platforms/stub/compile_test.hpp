#pragma once

#include "FastLED.h"

namespace fl {
static void stub_compile_tests() {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for stub platforms"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 1
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 1 for stub platforms"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1
#error "FASTLED_ALLOW_INTERRUPTS should be 1 for stub platforms"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for stub platforms"
#endif

// Stub platforms should define basic pin functions
#ifndef digitalPinToBitMask
#error "digitalPinToBitMask should be defined for stub platforms"
#endif

#ifndef digitalPinToPort
#error "digitalPinToPort should be defined for stub platforms"
#endif
}
}  // namespace fl
