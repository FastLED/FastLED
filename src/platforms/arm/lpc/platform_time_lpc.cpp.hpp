#pragma once

// IWYU pragma: private

// Time platform implementation for NXP LPC8xx platforms
// Currently provides no-op stubs - SysTick integration needed for real timing

#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "platforms/time_platform.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// TODO: Implement SysTick-based timing for LPC8xx
// For now, provide minimal stubs to allow compilation

void delay(fl::u32 ms) FL_NOEXCEPT {
    // Busy-wait stub - SysTick integration needed
    for (volatile fl::u32 i = 0; i < ms * 1000; i++) {
        __asm volatile("nop");
    }
}

void delayMicroseconds(fl::u32 us) FL_NOEXCEPT {
    // Busy-wait stub - SysTick integration needed
    for (volatile fl::u32 i = 0; i < us; i++) {
        __asm volatile("nop");
    }
}

fl::u32 millis() FL_NOEXCEPT {
    // TODO: Implement SysTick counter
    return 0;
}

fl::u32 micros() FL_NOEXCEPT {
    // TODO: Implement SysTick counter
    return 0;
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC
