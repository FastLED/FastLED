// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_ESP32_DELAYCYCLES_RISCV_H
#define __INC_FASTLED_PLATFORMS_ESP32_DELAYCYCLES_RISCV_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/esp/32/delaycycles_riscv.h
/// ESP32-C3/C6 (RISC-V) platform-specific cycle-accurate delay utilities

/// ESP32-C3/C6: RISC-V architecture uses mcycle CSR (Machine Cycle Counter)
FASTLED_FORCE_INLINE fl::u64 get_mcycle() {
  fl::u64 c;
  asm volatile("rdcycle %0" : "=r"(c));
  return c;
}

FASTLED_FORCE_INLINE void delay_cycles_mcycle(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u64 start = get_mcycle();
  while ((fl::u64)(get_mcycle() - start) < cycles) { }
}

#endif  // __INC_FASTLED_PLATFORMS_ESP32_DELAYCYCLES_RISCV_H
