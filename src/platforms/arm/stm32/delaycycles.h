// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_STM32_DELAYCYCLES_H
#define __INC_FASTLED_PLATFORMS_STM32_DELAYCYCLES_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/arm/stm32/delaycycles.h
/// ARM Cortex-M3/M4 (STM32) platform-specific cycle-accurate delay utilities

/// ARM Cortex-M3/M4 (STM32, Teensy 3.x, etc.): Use DWT cycle counter
constexpr fl::u32 ARM_DEMCR_ADDR = 0xE000EDFC;
constexpr fl::u32 ARM_DWT_CTRL_ADDR = 0xE0001000;
constexpr fl::u32 ARM_DWT_CYCCNT_ADDR = 0xE0001004;

FASTLED_FORCE_INLINE void dwt_enable_cycle_counter() {
  volatile fl::u32* demcr = (volatile fl::u32*)ARM_DEMCR_ADDR;
  volatile fl::u32* dwt_ctrl = (volatile fl::u32*)ARM_DWT_CTRL_ADDR;

  *demcr |= (1u << 24);  // DEMCR.TRCENA
  *dwt_ctrl |= 1u;       // DWT.CYCCNTENA
}

FASTLED_FORCE_INLINE fl::u32 dwt_cyccnt_arm() {
  volatile fl::u32* cyccnt = (volatile fl::u32*)ARM_DWT_CYCCNT_ADDR;
  return *cyccnt;
}

FASTLED_FORCE_INLINE void delay_cycles_dwt_arm(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = dwt_cyccnt_arm();
  while ((fl::u32)(dwt_cyccnt_arm() - start) < cycles) { }
}

#endif  // __INC_FASTLED_PLATFORMS_STM32_DELAYCYCLES_H
