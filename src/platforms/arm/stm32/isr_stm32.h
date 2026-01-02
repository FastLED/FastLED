/*
  FastLED — STM32 ISR Platform Header
  -----------------------------------
  Platform-specific ISR declarations for STM32.

  Supports timer-based and external interrupts using STM32 HAL/LL drivers.
  Uses TIM2-TIM5 for timer interrupts with NVIC priority configuration.

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

namespace fl {
namespace isr {
namespace platform {

// STM32 platform implementations
// Uses STM32 HAL timer API for periodic interrupts
// Uses STM32 HAL GPIO/EXTI for external interrupts
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
