// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_DELAYCYCLES_GENERIC_H
#define __INC_FASTLED_PLATFORMS_DELAYCYCLES_GENERIC_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/delaycycles_generic.h
/// Generic fallback cycle-accurate delay utilities for delay_cycles functions

/// Generic fallback: use tight NOP loop
FASTLED_FORCE_INLINE void delay_cycles_generic(fl::u32 cycles) {
  // Simple loop - not ideal but works on any platform
  while (cycles > 0) {
    __asm__ __volatile__("nop\n");
    cycles--;
  }
}

#endif  // __INC_FASTLED_PLATFORMS_DELAYCYCLES_GENERIC_H
