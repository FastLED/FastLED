
/// @file coroutine_runtime.cpp.hpp
/// Platform-specific coroutine runtime dispatch
///
/// This file includes the appropriate platform-specific coroutine runtime
/// implementation based on the detected platform.

// ok no namespace fl

#include "platforms/wasm/is_wasm.h"
#include "platforms/esp/is_esp.h"
#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_WASM)
    // WASM: No background coroutines, browser event loop handles scheduling
    #include "platforms/wasm/coroutine_runtime_wasm.hpp"
#elif defined(FASTLED_STUB_IMPL)
    // Stub/Host: Thread sleep_for + semaphore-based coroutine runner
    // Must check before ARDUINO because stub builds also define ARDUINO=10808
    #include "platforms/stub/coroutine_stub.hpp"
#elif defined(FL_IS_ESP32)
    // ESP32: FreeRTOS tasks and vTaskDelay
    #include "platforms/esp/32/coroutine_esp32.hpp"
#elif defined(FL_IS_TEENSY_4X)
    // Teensy: Cooperative ARM Cortex-M7 context switching
    #include "platforms/arm/teensy/coroutine_teensy.hpp"
#elif defined(ARDUINO)
    // Arduino (non-ESP32): No background coroutines — fallback
    #include "platforms/arduino/coroutine_runtime_arduino.hpp"
#else
    // Other platforms: Use thread sleep_for as fallback
    #include "platforms/stub/coroutine_stub.hpp"
#endif
