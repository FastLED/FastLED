// IWYU pragma: private

// ok no namespace fl

#pragma once

#include "fl/stl/stdint.h"

// CI13XX devices use a 32-bit Nuclei RISC-V core.
#define FASTLED_RISCV

// The Chipintelli Arduino core does not expose the AVR-style PINMAP macros.
// Hardware FastPin specializations are provided by fastpin_ci13xx.h instead.
#define FASTLED_NO_PINMAP

#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

typedef volatile fl::u32 RoReg;
typedef volatile fl::u32 RwReg;
