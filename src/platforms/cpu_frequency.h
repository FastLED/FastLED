#pragma once

#ifndef __INC_FASTLED_PLATFORMS_CPU_FREQUENCY_H
#define __INC_FASTLED_PLATFORMS_CPU_FREQUENCY_H

/// @file platforms/cpu_frequency.h
/// Platform-specific CPU frequency detection
/// Provides FL_CPU_FREQUENCY() macro for compile-time frequency determination

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

// ============================================================================
// CPU frequency macro for compile-time frequency determination
// ============================================================================

// Helper macro to get compile-time CPU frequency
// Note: Uses preprocessor for compatibility with constexpr functions in C++11
#if defined(STM32F2XX)
  // STM32F2: 120 MHz
  #define FL_CPU_FREQUENCY() 120000000UL
#elif defined(STM32F4)
  // STM32F4: 100 MHz
  #define FL_CPU_FREQUENCY() 100000000UL
#elif defined(STM32F1) || defined(__STM32F1__) || defined(STM32F10X_MD)
  // STM32F1: 72 MHz
  #define FL_CPU_FREQUENCY() 72000000UL
#elif defined(STM32G0xx)
  // STM32G0: 64 MHz
  #define FL_CPU_FREQUENCY() 64000000UL
#elif defined(STM32U5) || defined(STM32U5xx) || defined(STM32U585xx) || \
      defined(STM32U585XX) || defined(ARDUINO_UNO_Q) || \
      defined(CONFIG_BOARD_ARDUINO_UNO_Q) || defined(CONFIG_SOC_STM32U585XX)
  // STM32U5: UNO Q / U585 compile targets run the STM32duino proxy at 160 MHz.
  #define FL_CPU_FREQUENCY() 160000000UL
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  // Other ARM Cortex-M3/M4: check F_CPU or use conservative 72 MHz default
  #if defined(F_CPU) && (F_CPU > 0)
    #define FL_CPU_FREQUENCY() F_CPU
  #else
    #define FL_CPU_FREQUENCY() 72000000UL
  #endif
#elif defined(F_CPU)
  #define FL_CPU_FREQUENCY() F_CPU
#elif defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
  // ESP-IDF style (can be tuned via menuconfig)
  #define FL_CPU_FREQUENCY() ((fl::u32)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL)
#elif defined(ESP32)
  // Fallback for ESP32/ESP32-C3/ESP32-C6: assume 160 MHz (conservative default)
  #define FL_CPU_FREQUENCY() 160000000UL
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040: standard 125 MHz
  #define FL_CPU_FREQUENCY() 125000000UL
#elif defined(NRF52_SERIES)
  // nRF52: standard 64 MHz
  #define FL_CPU_FREQUENCY() 64000000UL
#elif defined(ARDUINO_ARCH_SAMD)
  // SAMD21: default 48 MHz, SAMD51: 120 MHz (use conservative default)
  #define FL_CPU_FREQUENCY() 48000000UL
#else
  // Fallback: assume 16 MHz (common Arduino)
  #define FL_CPU_FREQUENCY() 16000000UL
#endif

// Deprecated spelling. This header is reachable from user sketches through
// platforms.h, so `GET_CPU_FREQUENCY()` -- an unprefixed name in the global
// namespace -- may appear in existing sketches. Keep it working while new
// code moves to `FL_CPU_FREQUENCY()`.
//
// Defined with #ifndef so a sketch or vendor header that already claimed the
// generic name keeps its own definition rather than getting silently
// overridden by ours; that collision risk is precisely why the FL_ prefix
// exists.
#ifndef GET_CPU_FREQUENCY
  #define GET_CPU_FREQUENCY() FL_CPU_FREQUENCY()
#endif

namespace fl {

// ============================================================================
// ESP32-specific CPU frequency query (runtime detection)
// ============================================================================

#if defined(ESP32)
/// Get the current ESP32 CPU frequency at runtime
/// @return CPU frequency in Hz
u32 esp_clk_cpu_freq_impl() FL_NO_EXCEPT;
#endif

} // namespace fl

#endif // __INC_FASTLED_PLATFORMS_CPU_FREQUENCY_H
