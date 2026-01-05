/// @file platforms/arm/stm32/isr_stm32.hpp
/// @brief STM32 ISR timer implementation using STM32 HAL

#pragma once

#ifdef FL_IS_STM32

#include "fl/isr.h"
#include "fl/dbg.h"
#include "fl/warn.h"

namespace fl {
namespace isr {
namespace platform {

// Platform ID for STM32
// Platform ID registry: 0=STUB, 1=ESP32, 2=AVR, 3=NRF52, 4=RP2040, 5=Teensy, 6=STM32, 7=SAMD, 255=NULL
constexpr uint8_t STM32_PLATFORM_ID = 6;

int attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
    (void)config;
    (void)out_handle;
    FL_WARN("STM32 ISR: not yet implemented");
    return -100;  // Not implemented
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    (void)pin;
    (void)config;
    (void)out_handle;
    FL_WARN("STM32 ISR: not yet implemented");
    return -100;  // Not implemented
}

int detach_handler(isr_handle_t& handle) {
    (void)handle;
    FL_WARN("STM32 ISR: not yet implemented");
    return -100;  // Not implemented
}

int enable_handler(const isr_handle_t& handle) {
    (void)handle;
    FL_WARN("STM32 ISR: not yet implemented");
    return -100;  // Not implemented
}

int disable_handler(const isr_handle_t& handle) {
    (void)handle;
    FL_WARN("STM32 ISR: not yet implemented");
    return -100;  // Not implemented
}

bool is_handler_enabled(const isr_handle_t& handle) {
    (void)handle;
    return false;
}

const char* get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -100: return "Not implemented";
        default: return "Unknown error";
    }
}

const char* get_platform_name() {
    return "STM32 (stub)";
}

uint32_t get_max_timer_frequency() {
    return 1000000;  // 1 MHz placeholder
}

uint32_t get_min_timer_frequency() {
    return 1;  // 1 Hz placeholder
}

uint8_t get_max_priority() {
    return 15;  // STM32 NVIC supports 16 priority levels (0-15)
}

bool requires_assembly_handler(uint8_t priority) {
    (void)priority;
    return false;
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// Disable interrupts on ARM Cortex-M (STM32)
inline void interruptsDisable() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

/// Enable interrupts on ARM Cortex-M (STM32)
inline void interruptsEnable() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

} // namespace fl

#endif // FL_IS_STM32
