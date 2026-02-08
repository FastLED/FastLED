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
// implementations that call fl::isr::platforms::* functions.
//
// We provide non-inline wrappers here to ensure the functions are available for linking.

namespace fl {
namespace isr {

// Non-inline implementations (delegate to platform-specific inline functions via explicit instantiation)
// These are needed because inline functions in headers won't be available to other translation units

int attachTimerHandler(const isr_config_t& config, isr_handle_t* handle) {
    return isr::platforms::attach_timer_handler(config, handle);
}

int attachExternalHandler(u8 pin, const isr_config_t& config, isr_handle_t* handle) {
    return isr::platforms::attach_external_handler(pin, config, handle);
}

int detachHandler(isr_handle_t& handle) {
    return isr::platforms::detach_handler(handle);
}

int enableHandler(isr_handle_t& handle) {
    return isr::platforms::enable_handler(handle);
}

int disableHandler(isr_handle_t& handle) {
    return isr::platforms::disable_handler(handle);
}

bool isHandlerEnabled(const isr_handle_t& handle) {
    return isr::platforms::is_handler_enabled(handle);
}

const char* getErrorString(int error_code) {
    return isr::platforms::get_error_string(error_code);
}

const char* getPlatformName() {
    return isr::platforms::get_platform_name();
}

u32 getMaxTimerFrequency() {
    return isr::platforms::get_max_timer_frequency();
}

u32 getMinTimerFrequency() {
    return isr::platforms::get_min_timer_frequency();
}

u8 getMaxPriority() {
    return isr::platforms::get_max_priority();
}

bool requiresAssemblyHandler(u8 priority) {
    return isr::platforms::requires_assembly_handler(priority);
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
