#pragma once

#include "FastLED.h"

void wasm_tests() {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for WASM"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 1
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 1 for WASM"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1
#error "FASTLED_ALLOW_INTERRUPTS should be 1 for WASM"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for WASM"
#endif

// WASM should have a high F_CPU since it's not real hardware
#if F_CPU < 1000000
#error "WASM F_CPU should be reasonably high (not real hardware constraint)"
#endif

// Check WASM-specific defines
#ifndef FASTLED_STUB_IMPL
#error "FASTLED_STUB_IMPL should be defined for WASM"
#endif

#ifndef digitalPinToBitMask
#error "digitalPinToBitMask should be defined for WASM"
#endif
}
