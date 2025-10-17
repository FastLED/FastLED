#pragma once

#ifndef __INC_FASTLED_PLATFORMS_RP2040_DELAY_H
#define __INC_FASTLED_PLATFORMS_RP2040_DELAY_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/arm/rp/rp2040/delay.h
/// RP2040 platform-specific nanosecond-precision delay utilities

/// RP2040: Pico SDK provides busy_wait_at_least_cycles
/// or we can implement using the timer peripheral
extern "C" void busy_wait_at_least_cycles(fl::u32);

FASTLED_FORCE_INLINE void delay_cycles_pico(fl::u32 cycles) {
  busy_wait_at_least_cycles(cycles);
}

#endif // __INC_FASTLED_PLATFORMS_RP2040_DELAY_H
