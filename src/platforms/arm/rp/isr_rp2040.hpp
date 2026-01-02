/*
  FastLED — RP2040 ISR Platform Header
  ------------------------------------
  Platform-specific ISR declarations for RP2040.

  RP2040 Hardware:
  - Single 64-bit microsecond counter (1 MHz)
  - 4 hardware alarms (ALARM0-ALARM3) that match lower 32 bits of counter
  - 4 separate timer interrupts (TIMER_IRQ_0 through TIMER_IRQ_3)
  - GPIO interrupts on all 30 pins
  - NVIC priority system (lower number = higher priority, 0-255 range)

  Timer Capabilities:
  - Maximum alarm period: 2^32 microseconds (~71.58 minutes)
  - Minimum period: ~1 microsecond (limited by ISR overhead)
  - Frequency range: ~0.00023 Hz to 1 MHz
  - All alarms share the same 1 MHz counter

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

namespace fl {
namespace isr {
namespace platform {

// RP2040 platform implementations
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
