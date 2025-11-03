#pragma once

#ifndef __INC_FASTLED_PLATFORMS_ESP_DELAY_CYCLES_ESP32_H
#define __INC_FASTLED_PLATFORMS_ESP_DELAY_CYCLES_ESP32_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/esp/delay_cycles_esp32.h
/// ESP32 platform-specific cycle-accurate delay specializations

// Note: This file is intended to be included inside namespace fl
// namespace fl {

// ============================================================================
// NOP macros for ESP32
// ============================================================================

/// Single no-operation instruction for delay
#define FL_NOP __asm__ __volatile__("nop\n");
/// Double no-operation instruction for delay
#define FL_NOP2 __asm__ __volatile__("nop\n\t nop\n");

// ============================================================================
// ESP32-specific delaycycles specializations
// ============================================================================

/// Forward declaration
template<fl::cycle_t CYCLES>
inline void delaycycles();

/// ESP32 specialization for very large cycle counts
/// This prevents stack overflow on ESP32 with cycles = 4294966398
template<>
FASTLED_FORCE_INLINE void delaycycles<4294966398>() {
  const fl::u32 termination = 4294966398 / 10;
  const fl::u32 remainder = 4294966398 % 10;
  for (fl::u32 i = 0; i < termination; i++) {
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
    FL_NOP;
  }

  // Handle remainder cycles
  switch (remainder) {
    case 9:
      FL_NOP;
    case 8:
      FL_NOP;
    case 7:
      FL_NOP;
    case 6:
      FL_NOP;
    case 5:
      FL_NOP;
    case 4:
      FL_NOP;
    case 3:
      FL_NOP;
    case 2:
      FL_NOP;
    case 1:
      FL_NOP;
  }
}

// }  // namespace fl (closed by including file)

#endif  // __INC_FASTLED_PLATFORMS_ESP_DELAY_CYCLES_ESP32_H
