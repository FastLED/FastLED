/*
  FastLED — WebAssembly/Emscripten ISR Implementation
  --------------------------------------------------
  ISR implementation for Emscripten/WebAssembly platform.

  Note: Emscripten is single-threaded and does not support interrupts or timers
  at the C++ level. ISR operations are not possible and will assert at runtime.

  For animations requiring timing, use asyncify patterns or JavaScript callbacks
  instead of ISRs.

  License: MIT (FastLED)
*/

#ifdef __EMSCRIPTEN__

#include "fl/isr.h"
#include "fl/compiler_control.h"
#include "fl/assert.h"
#include "fl/namespace.h"

namespace fl {
namespace isr {

// =============================================================================
// WebAssembly ISR Implementation Class
// =============================================================================

/**
 * WebAssembly implementation that explicitly rejects ISR operations.
 * Emscripten is single-threaded and does not support interrupts.
 */
class WasmIsrImpl : public IsrImpl {
public:
    WasmIsrImpl() = default;
    ~WasmIsrImpl() override = default;

    int attachTimerHandler(const isr_config_t& config, isr_handle_t* out_handle) override {
        (void)config;
        if (out_handle) {
            *out_handle = isr_handle_t();
        }
        FL_ASSERT(false, "ISR not supported in emscripten");
        return -1;
    }

    int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) override {
        (void)pin;
        (void)config;
        if (out_handle) {
            *out_handle = isr_handle_t();
        }
        FL_ASSERT(false, "ISR not supported in emscripten");
        return -1;
    }

    int detachHandler(isr_handle_t& handle) override {
        handle = isr_handle_t();
        FL_ASSERT(false, "ISR not supported in emscripten");
        return -1;
    }

    int enableHandler(const isr_handle_t& handle) override {
        (void)handle;
        FL_ASSERT(false, "ISR not supported in emscripten");
        return -1;
    }

    int disableHandler(const isr_handle_t& handle) override {
        (void)handle;
        FL_ASSERT(false, "ISR not supported in emscripten");
        return -1;
    }

    bool isHandlerEnabled(const isr_handle_t& handle) override {
        (void)handle;
        FL_ASSERT(false, "ISR not supported in emscripten");
        return false;
    }

    const char* getErrorString(int error_code) override {
        (void)error_code;
        return "ISR not supported in emscripten (single-threaded environment)";
    }

    const char* getPlatformName() override {
        return "Emscripten/WebAssembly";
    }

    uint32_t getMaxTimerFrequency() override {
        return 0;
    }

    uint32_t getMinTimerFrequency() override {
        return 0;
    }

    uint8_t getMaxPriority() override {
        return 0;
    }

    bool requiresAssemblyHandler(uint8_t priority) override {
        (void)priority;
        return false;
    }
};

// =============================================================================
// Factory Function (Strong Symbol Override)
// =============================================================================

/**
 * Returns the WebAssembly ISR implementation.
 * This function provides a strong symbol that overrides the weak default
 * in isr_null.cpp when linking for Emscripten targets.
 */
IsrImpl& IsrImpl::get_instance() {
    static WasmIsrImpl wasm_impl;
    return wasm_impl;
}

} // namespace isr
} // namespace fl

#endif // __EMSCRIPTEN__
