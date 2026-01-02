/// @file platforms/arm/teensy/isr_teensy.cpp
/// @brief Teensy ISR timer implementation using IntervalTimer

#if FL_IS_TEENSY

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
    bool enabled;
    bool is_timer;  // true for timer, false for external interrupt

    teensy_isr_handle_data()
        : handler(nullptr)
        , user_data(nullptr)
        , frequency_hz(0)
        , enabled(false)
        , is_timer(false) {}
};

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
    if (config.priority != ISR_PRIORITY_DEFAULT) {
        // Teensy priority: 0-255, where 0 is highest
        // Our priority: 1-7, where 1 is lowest, 7 is highest
        // Map: priority 1 -> 255 (lowest), priority 7 -> 0 (highest)
        uint8_t teensy_priority = 255 - ((config.priority - 1) * 255 / 6);
        data->timer.priority(teensy_priority);
    }

    data->enabled = true;

    // Fill in handle
    if (handle) {
        handle->platform_handle = data;
        handle->handler = config.handler;
        handle->user_data = config.user_data;
        handle->platform_id = 1;  // Teensy platform ID
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
#if FL_IS_TEENSY_LC
    return "Teensy LC";
#elif FL_IS_TEENSY_30
    return "Teensy 3.0";
#elif FL_IS_TEENSY_31 || FL_IS_TEENSY_32
    return "Teensy 3.1/3.2";
#elif FL_IS_TEENSY_35
    return "Teensy 3.5";
#elif FL_IS_TEENSY_36
    return "Teensy 3.6";
#elif FL_IS_TEENSY_40
    return "Teensy 4.0";
#elif FL_IS_TEENSY_41
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
} // namespace fl

#endif // FL_IS_TEENSY
