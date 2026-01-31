// ok no namespace fl
#pragma once

/// @file interrupts_stm32_inline.h
/// @brief Lightweight inline interrupt control for STM32 (ARM Cortex-M)
///
/// This header provides only the inline interrupt enable/disable functions.
/// For full ISR functionality, include fl/isr.h instead.
///
/// These functions use ARM Cortex-M CPSID/CPSIE instructions to directly
/// manipulate the PRIMASK register.

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32
#error "This header is only for STM32 platforms"
#endif

namespace fl {

/// Disable interrupts on ARM Cortex-M (STM32)
/// Uses CPSID instruction to set PRIMASK, blocking all exceptions except NMI and HardFault
inline void interruptsDisable() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

/// Enable interrupts on ARM Cortex-M (STM32)
/// Uses CPSIE instruction to clear PRIMASK, enabling all interrupts
inline void interruptsEnable() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

} // namespace fl
