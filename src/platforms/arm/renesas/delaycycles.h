// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_RENESAS_DELAYCYCLES_H
#define __INC_FASTLED_PLATFORMS_RENESAS_DELAYCYCLES_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/arm/renesas/delaycycles.h
/// Renesas RA platform-specific cycle-accurate delay utilities
/// Used by Arduino UNO R4 WiFi (RA4M1) and other Renesas RA boards

/// Renesas RA (Cortex-M4): Use DWT cycle counter
/// Requires enabling DWT first (done once at init)
constexpr fl::u32 RENESAS_DEMCR_ADDR = 0xE000EDFC;
constexpr fl::u32 RENESAS_DWT_CTRL_ADDR = 0xE0001000;
constexpr fl::u32 RENESAS_DWT_CYCCNT_ADDR = 0xE0001004;

FASTLED_FORCE_INLINE void dwt_enable_cycle_counter() {
  // Enable trace and DWT
  volatile fl::u32* demcr = (volatile fl::u32*)RENESAS_DEMCR_ADDR;
  volatile fl::u32* dwt_ctrl = (volatile fl::u32*)RENESAS_DWT_CTRL_ADDR;

  *demcr |= (1u << 24);  // DEMCR.TRCENA = 1
  *dwt_ctrl |= 1u;       // DWT.CYCCNTENA = 1
}

FASTLED_FORCE_INLINE fl::u32 dwt_cyccnt() {
  volatile fl::u32* cyccnt = (volatile fl::u32*)RENESAS_DWT_CYCCNT_ADDR;
  return *cyccnt;
}

FASTLED_FORCE_INLINE void delay_cycles_dwt_renesas(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = dwt_cyccnt();
  while ((fl::u32)(dwt_cyccnt() - start) < cycles) { }
}

#endif  // __INC_FASTLED_PLATFORMS_RENESAS_DELAYCYCLES_H
