/// @file platforms/arm/teensy/isr_teensy.hpp
/// @brief Teensy ISR timer implementation using IntervalTimer
///
/// Priority Handling:
/// - Teensy boards use ARM Cortex-M4/M7 with NVIC (Nested Vectored Interrupt Controller)
/// - NVIC implements 4 priority bits (__NVIC_PRIO_BITS = 4), providing 16 priority levels
/// - Valid NVIC priorities: 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240
/// - Lower NVIC values = higher priority (0 is highest, 240 is lowest)
/// - FastLED ISR API uses priority 1-7 (1=lowest, 7=highest)
/// - Mapping: ISR priority N -> NVIC priority (16 - N*2)*16
/// - Examples: ISR 1->NVIC 224, ISR 4->NVIC 128, ISR 7->NVIC 32
///
/// Hardware Limitations:
/// - Teensy 4.x (i.MX RT1062): All PIT timers share IRQ_PIT, so priority is global
/// - Teensy 3.x (Kinetis): Each timer has separate NVIC slot, individual priorities work

#pragma once

#if defined(FL_IS_TEENSY)

#include "fl/isr.h"
#include "fl/warn.h"
#include <IntervalTimer.h>

namespace fl {
namespace isr {

// Platform-specific handle data
struct teensy_isr_handle_data {
    IntervalTimer timer;
    isr_handler_t handler;
    void* user_data;
    uint32_t frequency_hz;
    uint8_t nvic_priority;  // Stored NVIC priority for re-enable
    bool enabled;
    bool is_timer;  // true for timer, false for external interrupt

    teensy_isr_handle_data()
        : handler(nullptr)
        , user_data(nullptr)
        , frequency_hz(0)
        , nvic_priority(128)  // Default NVIC priority
        , enabled(false)
        , is_timer(false) {}
};

// Platform ID for Teensy
// Platform ID registry: 0=STUB, 1=ESP32, 2=AVR, 3=NRF52, 4=RP2040, 5=Teensy, 6=STM32, 7=SAMD, 255=NULL
constexpr uint8_t TEENSY_PLATFORM_ID = 5;

// Helper to convert isr_handle_t to platform data
static teensy_isr_handle_data* get_handle_data(const isr_handle_t& handle) {
    return static_cast<teensy_isr_handle_data*>(handle.platform_handle);
}

// Wrapper ISR that calls user handler
static void teensy_timer_isr_wrapper() {
    // Note: Teensy doesn't provide user_data in ISR callback, so we need static storage
    // This is handled by storing one handle per timer instance
    // For now, we'll use the IntervalTimer's internal mechanism
    // The actual user handler will be called via the platform_handle lookup
}

// Global timer data pointer
// Note: Teensy IntervalTimer API only supports one active timer at a time
// due to lack of user_data parameter in ISR callback. This limitation means
// only a single timer can be registered and active simultaneously.
//
// Additional Teensy 4.x limitation: All four PIT timers (0-3) share a single
// interrupt slot (IRQ_PIT), so they cannot have individual priorities. The
// IntervalTimer implementation selects the highest priority among all active
// timers and applies it globally. This is a hardware limitation of the i.MX
// RT1062 processor. Teensy 3.x boards do not have this limitation.
static teensy_isr_handle_data* g_active_timer_data = nullptr;

// Actual ISR wrapper that has access to handle data
static void teensy_isr_trampoline() {
    if (g_active_timer_data && g_active_timer_data->handler) {
        g_active_timer_data->handler(g_active_timer_data->user_data);
    }
}

// ============================================================================
// Platform-specific API implementation
// ============================================================================

int teensy_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle) {
    if (!config.handler) {
        return -1;  // Invalid handler
    }

    if (config.frequency_hz == 0) {
        return -2;  // Invalid frequency
    }

    // Check frequency bounds (Teensy IntervalTimer supports ~1 Hz to ~150 kHz typical)
    if (config.frequency_hz > 150000) {
        FL_WARN("Teensy timer frequency " << config.frequency_hz << " Hz may be too high (max ~150 kHz)");
    }

    // Allocate platform handle data
    teensy_isr_handle_data* data = new teensy_isr_handle_data();
    if (!data) {
        return -3;  // Allocation failed
    }

    data->handler = config.handler;
    data->user_data = config.user_data;
    data->frequency_hz = config.frequency_hz;
    data->is_timer = true;

    // Calculate interval in microseconds
    uint32_t interval_us = 1000000 / config.frequency_hz;

    // Store as global for ISR access (limitation of IntervalTimer API)
    g_active_timer_data = data;

    // Begin timer
    if (!data->timer.begin(teensy_isr_trampoline, interval_us)) {
        delete data;
        g_active_timer_data = nullptr;
        return -4;  // Timer begin failed
    }

    // Set priority if specified
    // Teensy NVIC implementation: 4 priority bits (16 levels), values are multiples of 16
    // Valid NVIC values: 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240
    // ISR API priority: 1-7, where 1 is lowest, 7 is highest
    // Map: ISR priority 1 -> NVIC 240 (lowest), ISR priority 7 -> NVIC 16 (highest user priority)
    // Formula: NVIC = (16 - priority * 2) * 16
    // Priority 1: (16 - 2) * 16 = 224
    // Priority 2: (16 - 4) * 16 = 192
    // Priority 3: (16 - 6) * 16 = 160
    // Priority 4: (16 - 8) * 16 = 128
    // Priority 5: (16 - 10) * 16 = 96
    // Priority 6: (16 - 12) * 16 = 64
    // Priority 7: (16 - 14) * 16 = 32

    // Clamp priority to valid range [1, 7]
    uint8_t priority = config.priority;
    if (priority < 1) priority = 1;
    if (priority > 7) priority = 7;

    // Map to Teensy NVIC priority (using stepped mapping across available range)
    uint8_t teensy_priority = (16 - priority * 2) * 16;
    data->nvic_priority = teensy_priority;  // Store for later use
    data->timer.priority(teensy_priority);

    data->enabled = true;

    // Fill in handle
    if (handle) {
        handle->platform_handle = data;
        handle->handler = config.handler;
        handle->user_data = config.user_data;
        handle->platform_id = TEENSY_PLATFORM_ID;
    }

    return 0;  // Success
}

int teensy_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    // External interrupts on Teensy use attachInterrupt() from Arduino core
    // Not implemented yet - return "not implemented"
    (void)pin;
    (void)config;
    (void)handle;
    return -100;  // Not implemented
}

int teensy_detach_handler(isr_handle_t& handle) {
    teensy_isr_handle_data* data = get_handle_data(handle);
    if (!data) {
        return -1;  // Invalid handle
    }

    if (data->is_timer) {
        data->timer.end();

        // Clear global if this is the active timer
        if (g_active_timer_data == data) {
            g_active_timer_data = nullptr;
        }
    }

    delete data;
    handle.platform_handle = nullptr;
    handle.handler = nullptr;
    handle.user_data = nullptr;

    return 0;  // Success
}

int teensy_enable_handler(const isr_handle_t& handle) {
    teensy_isr_handle_data* data = get_handle_data(handle);
    if (!data) {
        return -1;  // Invalid handle
    }

    if (data->enabled) {
        return 0;  // Already enabled
    }

    if (data->is_timer) {
        uint32_t interval_us = 1000000 / data->frequency_hz;
        // Restore global pointer for ISR access
        g_active_timer_data = data;
        if (!data->timer.begin(teensy_isr_trampoline, interval_us)) {
            return -2;  // Failed to restart
        }
        // Restore priority setting
        data->timer.priority(data->nvic_priority);
        data->enabled = true;
    }

    return 0;  // Success
}

int teensy_disable_handler(const isr_handle_t& handle) {
    teensy_isr_handle_data* data = get_handle_data(handle);
    if (!data) {
        return -1;  // Invalid handle
    }

    if (!data->enabled) {
        return 0;  // Already disabled
    }

    if (data->is_timer) {
        data->timer.end();
        // Clear global pointer if this is the active timer
        if (g_active_timer_data == data) {
            g_active_timer_data = nullptr;
        }
        data->enabled = false;
    }

    return 0;  // Success
}

bool teensy_is_handler_enabled(const isr_handle_t& handle) {
    teensy_isr_handle_data* data = get_handle_data(handle);
    if (!data) {
        return false;
    }
    return data->enabled;
}

const char* teensy_get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "Invalid handler or handle";
        case -2: return "Invalid frequency or failed to restart";
        case -3: return "Memory allocation failed";
        case -4: return "Timer begin failed";
        case -100: return "Not implemented (external interrupts)";
        default: return "Unknown error";
    }
}

const char* teensy_get_platform_name() {
#if defined(FL_IS_TEENSY_LC)
    return "Teensy LC";
#elif defined(FL_IS_TEENSY_30)
    return "Teensy 3.0";
#elif defined(FL_IS_TEENSY_31) || defined(FL_IS_TEENSY_32)
    return "Teensy 3.1/3.2";
#elif defined(FL_IS_TEENSY_35)
    return "Teensy 3.5";
#elif defined(FL_IS_TEENSY_36)
    return "Teensy 3.6";
#elif defined(FL_IS_TEENSY_40)
    return "Teensy 4.0";
#elif defined(FL_IS_TEENSY_41)
    return "Teensy 4.1";
#else
    return "Teensy (unknown variant)";
#endif
}

uint32_t teensy_get_max_timer_frequency() {
    // Conservative estimate for all Teensy variants
    return 150000;  // 150 kHz
}

uint32_t teensy_get_min_timer_frequency() {
    return 1;  // 1 Hz minimum
}

uint8_t teensy_get_max_priority() {
    return 7;  // Teensy supports 0-255, but we map to 1-7 range
}

bool teensy_requires_assembly_handler(uint8_t priority) {
    (void)priority;
    return false;  // Teensy IntervalTimer handles ISR registration internally
}

// fl::isr::platform namespace wrappers (call fl::isr:: functions)
namespace platform {

int attach_timer_handler(const isr_config_t& config, isr_handle_t* handle) {
    return teensy_attach_timer_handler(config, handle);
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    return teensy_attach_external_handler(pin, config, handle);
}

int detach_handler(isr_handle_t& handle) {
    return teensy_detach_handler(handle);
}

int enable_handler(const isr_handle_t& handle) {
    return teensy_enable_handler(handle);
}

int disable_handler(const isr_handle_t& handle) {
    return teensy_disable_handler(handle);
}

bool is_handler_enabled(const isr_handle_t& handle) {
    return teensy_is_handler_enabled(handle);
}

const char* get_error_string(int error_code) {
    return teensy_get_error_string(error_code);
}

const char* get_platform_name() {
    return teensy_get_platform_name();
}

uint32_t get_max_timer_frequency() {
    return teensy_get_max_timer_frequency();
}

uint32_t get_min_timer_frequency() {
    return teensy_get_min_timer_frequency();
}

uint8_t get_max_priority() {
    return teensy_get_max_priority();
}

bool requires_assembly_handler(uint8_t priority) {
    return teensy_requires_assembly_handler(priority);
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// Disable interrupts on ARM Cortex-M (Teensy)
inline void interruptsDisable() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

/// Enable interrupts on ARM Cortex-M (Teensy)
inline void interruptsEnable() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

} // namespace fl

#endif // FL_IS_TEENSY
