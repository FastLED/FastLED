/*
  FastLED — Null ISR Implementation (Weak Symbols)
  ------------------------------------------------
  Default null implementation of the cross-platform ISR API using weak symbols.
  This provides a fallback when no platform-specific implementation is available.

  Platform-specific implementations (ESP32, Teensy, AVR, etc.) provide strong
  symbol overrides that replace these weak defaults at link time.

  License: MIT (FastLED)
*/

#include "fl/isr.h"
#include "fl/compiler_control.h"
#include "fl/namespace.h"

// Get ESP-IDF version macros if on ESP32
#if defined(ESP32)
    #include "platforms/esp/32/led_sysdefs_esp32.h"
#endif

namespace fl {
namespace isr {

// =============================================================================
// Null ISR Implementation Class
// =============================================================================

/**
 * Null implementation that provides safe no-op defaults.
 * Used when no platform-specific ISR implementation is available.
 */
class NullIsrImpl : public IsrImpl {
public:
    NullIsrImpl() = default;
    ~NullIsrImpl() override = default;

    int attachTimerHandler(const isr_config_t& config, isr_handle_t* out_handle) override {
        (void)config;
        if (out_handle) {
            *out_handle = isr_handle_t();  // Invalid handle
        }
        return -100;  // Not implemented error
    }

    int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) override {
        (void)pin;
        (void)config;
        if (out_handle) {
            *out_handle = isr_handle_t();  // Invalid handle
        }
        return -100;  // Not implemented error
    }

    int detachHandler(isr_handle_t& handle) override {
        handle = isr_handle_t();  // Invalidate handle
        return -100;  // Not implemented error
    }

    int enableHandler(const isr_handle_t& handle) override {
        (void)handle;
        return -100;  // Not implemented error
    }

    int disableHandler(const isr_handle_t& handle) override {
        (void)handle;
        return -100;  // Not implemented error
    }

    bool isHandlerEnabled(const isr_handle_t& handle) override {
        (void)handle;
        return false;
    }

    const char* getErrorString(int error_code) override {
        switch (error_code) {
            case 0: return "Success";
            case -100: return "Not implemented (no platform ISR support)";
            default: return "Unknown error";
        }
    }

    const char* getPlatformName() override {
        return "Null";
    }

    uint32_t getMaxTimerFrequency() override {
        return 0;  // No timer support
    }

    uint32_t getMinTimerFrequency() override {
        return 0;  // No timer support
    }

    uint8_t getMaxPriority() override {
        return 0;  // No priority support
    }

    bool requiresAssemblyHandler(uint8_t priority) override {
        (void)priority;
        return false;
    }
};

// =============================================================================
// Weak Symbol Factory Function (Static Member)
// =============================================================================

// Only provide weak symbol if no platform implementation is available
// On Windows, weak symbols don't work reliably, so we use conditional compilation
// Note: ESP32 with ESP-IDF < 5.0 also uses the null implementation (no gptimer API)
#if !defined(STUB_PLATFORM) && !defined(FASTLED_STUB_IMPL)
    // For ESP32 platforms, check if we need the null implementation (ESP-IDF < 5.0)
    #if !defined(ESP32)
        // Not ESP32 - provide null implementation
    #elif defined(ESP32) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        // ESP32 with old ESP-IDF - provide null implementation
    #else
        // ESP32 with ESP-IDF >= 5.0 - skip null implementation (platform provides it)
        #define SKIP_NULL_ISR_IMPL
    #endif
#endif

#if !defined(STUB_PLATFORM) && !defined(FASTLED_STUB_IMPL) && !defined(SKIP_NULL_ISR_IMPL)

/**
 * Default factory that returns the null implementation.
 * Platform-specific implementations override this via conditional compilation.
 */
IsrImpl& IsrImpl::get_instance() {
    static NullIsrImpl null_impl;
    return null_impl;
}

#endif

// =============================================================================
// Public API Functions (forward to implementation)
// =============================================================================

int attachTimerHandler(const isr_config_t& config, isr_handle_t* handle) {
    return IsrImpl::get_instance().attachTimerHandler(config, handle);
}

int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    return IsrImpl::get_instance().attachExternalHandler(pin, config, handle);
}

int detachHandler(isr_handle_t& handle) {
    return IsrImpl::get_instance().detachHandler(handle);
}

int enableHandler(const isr_handle_t& handle) {
    return IsrImpl::get_instance().enableHandler(handle);
}

int disableHandler(const isr_handle_t& handle) {
    return IsrImpl::get_instance().disableHandler(handle);
}

bool isHandlerEnabled(const isr_handle_t& handle) {
    return IsrImpl::get_instance().isHandlerEnabled(handle);
}

const char* getErrorString(int error_code) {
    return IsrImpl::get_instance().getErrorString(error_code);
}

const char* getPlatformName() {
    return IsrImpl::get_instance().getPlatformName();
}

uint32_t getMaxTimerFrequency() {
    return IsrImpl::get_instance().getMaxTimerFrequency();
}

uint32_t getMinTimerFrequency() {
    return IsrImpl::get_instance().getMinTimerFrequency();
}

uint8_t getMaxPriority() {
    return IsrImpl::get_instance().getMaxPriority();
}

bool requiresAssemblyHandler(uint8_t priority) {
    return IsrImpl::get_instance().requiresAssemblyHandler(priority);
}

} // namespace isr
} // namespace fl
