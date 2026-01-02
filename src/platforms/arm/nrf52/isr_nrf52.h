/*
  FastLED — NRF52 ISR Platform Header
  -----------------------------------
  Platform-specific ISR declarations for NRF52.

  Timer Hardware:
  - NRF52 provides 5 TIMER instances (TIMER0-TIMER4)
  - TIMER0-2: 4 compare channels each
  - TIMER3-4: 6 compare channels each
  - Timer frequency: up to 16 MHz with configurable prescaler
  - Resolution: 1 MHz (1us) to 16 MHz (62.5ns) typical
  - Support for microsecond-level interrupts via nrfx_timer driver

  GPIO Interrupts:
  - GPIOTE (GPIO Tasks and Events) peripheral for external interrupts
  - 8 GPIOTE channels available
  - Support for edge and level triggering
  - Pin change detection via PORT event

  Priority Levels:
  - ARM Cortex-M4F NVIC with 3 priority bits (__NVIC_PRIO_BITS = 3)
  - Priority range: 0-7 (0 = highest, 7 = lowest)
  - All priorities support C handlers (no assembly required)
  - SoftDevice (BLE stack) reserves priorities 0-1 when enabled
  - Typical user range: 2-7 (priority 2 = highest for user)

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

namespace fl {
namespace isr {
namespace platform {

// NRF52 platform implementations
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
