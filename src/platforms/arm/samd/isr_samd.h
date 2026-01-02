/*
  FastLED — SAMD ISR Platform Header
  ----------------------------------
  Platform-specific ISR declarations for SAMD (Arduino Zero, MKR series, etc.).

  Hardware Support:
  - SAMD21 family: Cortex-M0+, 48 MHz, TC3/TC4/TC5 timers, EIC external interrupts
  - SAMD51 family: Cortex-M4F, 120 MHz, TC0-TC7 timers, EIC external interrupts

  Timer Hardware:
  - TC (Timer/Counter): 16-bit or 32-bit counter with prescaler and compare
  - Supports frequencies from ~1 Hz to ~24 MHz (SAMD21) or ~60 MHz (SAMD51)
  - Maximum direct period: ~1.4 seconds with 16-bit counter

  External Interrupts:
  - EIC (External Interrupt Controller): 16 external interrupt lines
  - Supports edge and level triggering
  - NVIC priority levels: 0-3 (0 = highest priority)

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

namespace fl {
namespace isr {
namespace platform {

// SAMD platform implementations
int attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int detach_handler(isr_handle_t& handle);
int enable_handler(const isr_handle_t& handle);
int disable_handler(const isr_handle_t& handle);
bool is_handler_enabled(const isr_handle_t& handle);
const char* get_error_string(int error_code);
const char* get_platform_name();
uint32_t get_max_timer_frequency();
uint32_t get_min_timer_frequency();
uint8_t get_max_priority();
bool requires_assembly_handler(uint8_t priority);

} // namespace platform
} // namespace isr
} // namespace fl
