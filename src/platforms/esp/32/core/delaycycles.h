// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_ESP32_DELAYCYCLES_H
#define __INC_FASTLED_PLATFORMS_ESP32_DELAYCYCLES_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/esp/32/delaycycles.h
/// ESP32 (Xtensa) platform-specific cycle-accurate delay utilities

/// ESP32 Xtensa: Use CCOUNT register (cycle counter)
/// Already included via platforms/esp/32/core/clock_cycles.h
#include "clock_cycles.h"

FASTLED_FORCE_INLINE fl::u32 get_ccount() {
  return __clock_cycles();
}

FASTLED_FORCE_INLINE void delay_cycles_ccount(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = get_ccount();
  while ((fl::u32)(get_ccount() - start) < cycles) { }
}

#endif  // __INC_FASTLED_PLATFORMS_ESP32_DELAYCYCLES_H
