#pragma once

// IWYU pragma: private

#include "fl/system/fastled.h"
#include "fl/stl/noexcept.h"

namespace fl {
static void stub_compile_tests() FL_NOEXCEPT {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for stub platforms"
#endif

#if !defined(SKETCH_HAS_LARGE_MEMORY_OVERRIDDEN)
#if SKETCH_HAS_LARGE_MEMORY != 1
#error "SKETCH_HAS_LARGE_MEMORY should be 1 for stub platforms"
#endif
#if SKETCH_HAS_HUGE_MEMORY != 1
#error "SKETCH_HAS_HUGE_MEMORY should be 1 for stub platforms"
#endif
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
