
/// @file coroutine_runtime.cpp.hpp
/// Platform-specific coroutine runtime dispatch
///
/// This file includes the appropriate platform-specific coroutine_runtime_*.hpp
/// implementation based on the detected platform.

// ok no namespace fl

#include "platforms/wasm/is_wasm.h"
#include "platforms/esp/is_esp.h"

#if defined(FL_IS_WASM)
    // WASM: Real sleep via emscripten_thread_sleep
    #include "platforms/wasm/coroutine_runtime_wasm.hpp"
#elif defined(FASTLED_STUB_IMPL)
    // Stub/Host: Use thread sleep_for + coroutine runner
    // Must check before ARDUINO because stub builds also define ARDUINO=10808
    #include "platforms/stub/coroutine_runtime_stub.hpp"
#elif defined(FL_IS_ESP32)
    // ESP32: Uses vTaskDelay to yield to FreeRTOS scheduler
    #include "platforms/esp/32/coroutine_runtime_esp32.hpp"
#elif defined(ARDUINO)
    // Arduino (non-ESP32): Use native delayMicroseconds
    #include "platforms/arduino/coroutine_runtime_arduino.hpp"
#else
    // Other platforms: Use thread sleep_for as fallback
    #include "platforms/stub/coroutine_runtime_stub.hpp"
#endif
