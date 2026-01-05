/*
  FastLED — Cross-Platform ISR Handler API - Implementation
  ---------------------------------------------------------
  Implementation of cross-platform ISR attachment functions.
  Dispatches to platform-specific implementations via platform headers.

  License: MIT (FastLED)
*/

#include "isr.h"
#include "platforms/isr.h"  // Platform dispatch header provides inline implementations

// The platform dispatch header (platforms/isr.h) selects the appropriate platform-specific
// header based on compile-time platform detection. Each platform header provides inline
// implementations that call fl::isr::platform::* functions.
//
// We provide non-inline wrappers here to ensure the functions are available for linking.

namespace fl {
namespace isr {

// Non-inline implementations (delegate to platform-specific inline functions via explicit instantiation)
// These are needed because inline functions in headers won't be available to other translation units

int attachTimerHandler(const isr_config_t& config, isr_handle_t* handle) {
    return isr::platform::attach_timer_handler(config, handle);
}

int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    return isr::platform::attach_external_handler(pin, config, handle);
}

int detachHandler(isr_handle_t& handle) {
    return isr::platform::detach_handler(handle);
}

int enableHandler(isr_handle_t& handle) {
    return isr::platform::enable_handler(handle);
}

int disableHandler(isr_handle_t& handle) {
    return isr::platform::disable_handler(handle);
}

bool isHandlerEnabled(const isr_handle_t& handle) {
    return isr::platform::is_handler_enabled(handle);
}

const char* getErrorString(int error_code) {
    return isr::platform::get_error_string(error_code);
}

const char* getPlatformName() {
    return isr::platform::get_platform_name();
}

uint32_t getMaxTimerFrequency() {
    return isr::platform::get_max_timer_frequency();
}

uint32_t getMinTimerFrequency() {
    return isr::platform::get_min_timer_frequency();
}

uint8_t getMaxPriority() {
    return isr::platform::get_max_priority();
}

bool requiresAssemblyHandler(uint8_t priority) {
    return isr::platform::requires_assembly_handler(priority);
}

// ============================================================================
// CriticalSection implementation
// ============================================================================

CriticalSection::CriticalSection() {
    interruptsDisable();
}

CriticalSection::~CriticalSection() {
    interruptsEnable();
}

} // namespace isr
} // namespace fl
