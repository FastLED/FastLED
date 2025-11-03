// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_SAMD_DELAYCYCLES_H
#define __INC_FASTLED_PLATFORMS_SAMD_DELAYCYCLES_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/arm/d21/delaycycles.h
/// SAMD platform-specific cycle-accurate delay utilities
/// Used by both SAMD21 (d21) and SAMD51 (d51)

/// SAMD (Cortex-M0+): Use DWT cycle counter (similar to nRF52)
constexpr fl::u32 SAMD_DEMCR_ADDR = 0xE000EDFC;
constexpr fl::u32 SAMD_DWT_CTRL_ADDR = 0xE0001000;
constexpr fl::u32 SAMD_DWT_CYCCNT_ADDR = 0xE0001004;

FASTLED_FORCE_INLINE void dwt_enable_cycle_counter() {
  volatile fl::u32* demcr = (volatile fl::u32*)SAMD_DEMCR_ADDR;
  volatile fl::u32* dwt_ctrl = (volatile fl::u32*)SAMD_DWT_CTRL_ADDR;

  *demcr |= (1u << 24);
  *dwt_ctrl |= 1u;
}

FASTLED_FORCE_INLINE fl::u32 dwt_cyccnt() {
  volatile fl::u32* cyccnt = (volatile fl::u32*)SAMD_DWT_CYCCNT_ADDR;
  return *cyccnt;
}

FASTLED_FORCE_INLINE void delay_cycles_dwt_samd(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = dwt_cyccnt();
  while ((fl::u32)(dwt_cyccnt() - start) < cycles) { }
}

#endif  // __INC_FASTLED_PLATFORMS_SAMD_DELAYCYCLES_H
