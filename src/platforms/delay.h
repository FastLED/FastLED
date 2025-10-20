#pragma once

#ifndef __INC_FASTLED_PLATFORMS_DELAY_H
#define __INC_FASTLED_PLATFORMS_DELAY_H

/// @file platforms/delay.h
/// Central distribution header for platform-specific delay implementations
/// This header handles all platform detection and includes the appropriate
/// platform-specific delay and delay_cycles headers.

#include "fl/types.h"
#include "fl/force_inline.h"

// ============================================================================
// Platform-specific nanosecond-precision delay includes
// ============================================================================

#if defined(ARDUINO_ARCH_AVR)
#include "platforms/avr/delay.h"
#elif defined(ESP32) && !defined(ESP32C3) && !defined(ESP32C6)
#include "platforms/esp/32/delay.h"
#elif defined(ESP32C3) || defined(ESP32C6)
#include "platforms/esp/32/delay_riscv.h"
#elif defined(ARDUINO_ARCH_RP2040)
#include "platforms/arm/rp/rp2040/delay.h"
#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/delay.h"
#elif defined(ARDUINO_ARCH_SAMD)
#include "platforms/arm/d21/delay.h"
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
#include "platforms/arm/stm32/delay.h"
#else
#include "platforms/delay_generic.h"
#endif

// ============================================================================
// Platform-specific cycle delay includes (delay_cycles)
// ============================================================================
// These provide NOP macros and delaycycles template implementations

#if defined(__AVR__)
#include "platforms/avr/delay_cycles.h"
#elif defined(ESP32)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/esp/delay_cycles_esp32.h"
#else
#include "platforms/shared/delay_cycles_generic.h"
#endif

namespace fl {

// Forward declarations for platform-specific delay implementations
/// Platform-specific implementation of nanosecond delay with runtime frequency
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz);

/// Platform-specific implementation of nanosecond delay with auto-detected frequency
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns);

}  // namespace fl

// ============================================================================
// ESP32 NOP workaround
// ============================================================================
// ESP32 core has its own definition of NOP, so undef it first
#ifdef ESP32
#undef NOP
#undef NOP2
#endif

#endif  // __INC_FASTLED_PLATFORMS_DELAY_H
