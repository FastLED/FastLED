// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

// IWYU pragma: private

#include "fl/system/fastled.h"
#include "fl/stl/noexcept.h"

namespace fl {
static void stub_compile_tests() FL_NO_EXCEPT {
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

#if defined(FL_IS_WASM)
#if F_CPU < 1000000
#error "WASM F_CPU should be reasonably high"
#endif

#ifndef FASTLED_STUB_IMPL
#error "FASTLED_STUB_IMPL should be defined for WASM"
#endif
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
