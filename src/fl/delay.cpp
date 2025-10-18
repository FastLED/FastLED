/// @file fl/delay.cpp
/// Implementation for delay utilities

#include "fl/stdint.h"
#include "fl/delay.h"
#include "fl/types.h"
#include "fl/compiler_control.h"

// ============================================================================
// Platform-specific includes
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

// ESP32 core has its own definition of NOP, so undef it first
#ifdef ESP32
#undef NOP
#undef NOP2
#endif

// Include platform-specific cycle delay implementations
#if defined(__AVR__)
#include "platforms/avr/delay_cycles.h"
#elif defined(ESP32)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/esp/delay_cycles_esp32.h"
#else
#include "platforms/shared/delay_cycles_generic.h"
#endif

// ============================================================================
// Platform-provided delay functions (from Arduino or platform layer)
// ============================================================================
extern "C" {
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
}

namespace fl {

// ============================================================================
// Runtime delayNanoseconds implementations
// ============================================================================

void delayNanoseconds(fl::u32 ns) {
  // Query runtime clock frequency
#if defined(ESP32)
  fl::u32 hz = esp_clk_cpu_freq_impl();
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040 clock is fixed at 125 MHz in normal mode
  fl::u32 hz = 125000000UL;
#elif defined(NRF52_SERIES)
  // nRF52 typically runs at 64 MHz
  fl::u32 hz = 64000000UL;
#elif defined(ARDUINO_ARCH_SAMD)
  // SAMD uses F_CPU if defined
  #if defined(F_CPU)
  fl::u32 hz = F_CPU;
  #else
  fl::u32 hz = 48000000UL;  // SAMD21 default
  #endif
#else
  // Fallback to F_CPU (most platforms)
  #if defined(F_CPU)
  fl::u32 hz = F_CPU;
  #else
  fl::u32 hz = 16000000UL;
  #endif
#endif

  fl::u32 cycles = detail::cycles_from_ns(ns, hz);

  if (cycles == 0) return;

  // Platform-specific implementation
#if defined(ARDUINO_ARCH_AVR)
  delay_cycles_avr_nop(cycles);
#elif defined(ESP32) && !defined(ESP32C3) && !defined(ESP32C6)
  delay_cycles_ccount(cycles);
#elif defined(ESP32C3) || defined(ESP32C6)
  delay_cycles_mcycle(cycles);
#elif defined(ARDUINO_ARCH_RP2040)
  delay_cycles_pico(cycles);
#elif defined(NRF52_SERIES)
  delay_cycles_dwt(cycles);
#elif defined(ARDUINO_ARCH_SAMD)
  delay_cycles_dwt_samd(cycles);
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  delay_cycles_dwt_arm(cycles);
#else
  delay_cycles_generic(cycles);
#endif
}

void delayNanoseconds(fl::u32 ns, fl::u32 hz) {
  fl::u32 cycles = detail::cycles_from_ns(ns, hz);

  if (cycles == 0) return;

  // Platform-specific implementation
#if defined(ARDUINO_ARCH_AVR)
  delay_cycles_avr_nop(cycles);
#elif defined(ESP32) && !defined(ESP32C3) && !defined(ESP32C6)
  delay_cycles_ccount(cycles);
#elif defined(ESP32C3) || defined(ESP32C6)
  delay_cycles_mcycle(cycles);
#elif defined(ARDUINO_ARCH_RP2040)
  delay_cycles_pico(cycles);
#elif defined(NRF52_SERIES)
  delay_cycles_dwt(cycles);
#elif defined(ARDUINO_ARCH_SAMD)
  delay_cycles_dwt_samd(cycles);
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  delay_cycles_dwt_arm(cycles);
#else
  delay_cycles_generic(cycles);
#endif
}

// ============================================================================
// delaycycles template specializations
// ============================================================================
// These pre-instantiated specializations optimize compilation time and code size
// for common cycle counts.

// Specializations for small negative and zero cycles
template<> FASTLED_FORCE_INLINE void delaycycles<-10>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-9>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-8>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-7>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-6>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-5>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-4>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-3>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-2>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-1>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<0>() {}

// Specializations for small positive cycles
template<> FASTLED_FORCE_INLINE void delaycycles<1>() { FL_NOP; }
template<> FASTLED_FORCE_INLINE void delaycycles<2>() { FL_NOP2; }
template<> FASTLED_FORCE_INLINE void delaycycles<3>() {
  FL_NOP;
  FL_NOP2;
}
template<> FASTLED_FORCE_INLINE void delaycycles<4>() {
  FL_NOP2;
  FL_NOP2;
}
template<> FASTLED_FORCE_INLINE void delaycycles<5>() {
  FL_NOP2;
  FL_NOP2;
  FL_NOP;
}

// Specialization for a gigantic amount of cycles on ESP32
#if defined(ESP32)
template<> FASTLED_FORCE_INLINE void delaycycles<4294966398>() {
  // specialization for a gigantic amount of cycles, apparently this is needed
  // or esp32 will blow the stack with cycles = 4294966398.
  const fl::u32 termination = 4294966398 / 10;
  const fl::u32 remainder = 4294966398 % 10;
  for (fl::u32 i = 0; i < termination; i++) {
    FL_NOP; FL_NOP; FL_NOP; FL_NOP; FL_NOP;
    FL_NOP; FL_NOP; FL_NOP; FL_NOP; FL_NOP;
  }

  // remainder
  switch (remainder) {
    case 9: FL_NOP;
    case 8: FL_NOP;
    case 7: FL_NOP;
    case 6: FL_NOP;
    case 5: FL_NOP;
    case 4: FL_NOP;
    case 3: FL_NOP;
    case 2: FL_NOP;
    case 1: FL_NOP;
  }
}
#endif

// ============================================================================
// Millisecond and Microsecond delay implementations
// ============================================================================

void delay(u32 ms) {
  ::delay((unsigned long)ms);
}

void delayMicroseconds(u32 us) {
  ::delayMicroseconds((unsigned int)us);
}

#if defined(ESP32)
// ============================================================================
// ESP32 CPU frequency query (C interop)
// ============================================================================

extern "C" {
  uint32_t esp_clk_cpu_freq();
}

/// Get the current ESP32 CPU frequency at runtime
uint32_t esp_clk_cpu_freq_impl() {
  return ::esp_clk_cpu_freq();
}

#endif  // defined(ESP32)

}  // namespace fl
