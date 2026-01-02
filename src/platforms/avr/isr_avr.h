/*
  FastLED — AVR ISR Platform Header
  ----------------------------------
  Platform-specific ISR declarations for AVR (Arduino Uno, Mega, etc.).

  Hardware Notes:
  - Uses Timer1 (16-bit timer) for precise timing
  - Frequency range: ~1 Hz to ~250 kHz (16 MHz CPU)
  - No hardware interrupt priority levels (all interrupts equal)
  - Supports ATmega328P (Uno), ATmega2560 (Mega), and other ATmega variants

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

namespace fl {
namespace isr {
namespace platform {

// AVR platform implementations
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
