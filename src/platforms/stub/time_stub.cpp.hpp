
// ok no namespace fl
#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "time_stub.h"
#include "fl/stl/function.h"
#include "fl/async.h"
#include <chrono>
#include <thread>

#if defined(FASTLED_USE_PTHREAD_DELAY) || defined(FASTLED_USE_PTHREAD_YIELD)
#include <time.h>
#include <errno.h>
#include <sched.h>
#endif

static auto start_time = std::chrono::system_clock::now();  // okay std namespace

// Global delay function override for fast testing
static fl::function<void(uint32_t)> g_delay_override;

extern "C" {

#if !defined(__EMSCRIPTEN__) && !defined(FASTLED_NO_ARDUINO_STUBS)
// STUB timing functions - excluded for WASM builds which provide their own implementations
// WASM timing functions are in src/platforms/wasm/timer.cpp and src/platforms/wasm/js.cpp
// Also excluded when FASTLED_NO_ARDUINO_STUBS is defined (for compatibility with ArduinoFake, etc.)

uint32_t millis() {
    auto current_time = std::chrono::system_clock::now();  // okay std namespace
    return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();  // okay std namespace
}

uint32_t micros() {
    auto current_time = std::chrono::system_clock::now();  // okay std namespace
    return std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time).count();  // okay std namespace
}

void delay(unsigned long ms) {
    // Use override function if set (for fast testing)
    if (g_delay_override) {
        g_delay_override(static_cast<uint32_t>(ms));
        return;
    }

    // unsigned long can't be negative, no check needed

    // Pump async runner during delay (matches WASM behavior)
    // This ensures coroutines and async tasks can execute during delay
    uint32_t end = millis() + ms;

    while (millis() < end) {
        // Update all async tasks (coroutines, timers, fetch, etc.)
        fl::async_run();

        if (millis() >= end) {
            break;
        }

        // Small sleep to avoid busy-wait (1ms granularity)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // okay std namespace
    }
}

void delayMicroseconds(unsigned long us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));  // okay std namespace
}

void yield() {
#ifdef FASTLED_USE_PTHREAD_YIELD
    // POSIX thread yield to allow other threads to run
    sched_yield();
#else
    std::this_thread::yield();  // okay std namespace
#endif
}

#endif // !__EMSCRIPTEN__ && !FASTLED_NO_ARDUINO_STUBS

} // extern "C"

// Function to set delay override (C++ linkage for test runner)
void setDelayFunction(const fl::function<void(uint32_t)>& delayFunc) {
    g_delay_override = delayFunc;
}

#endif  // FASTLED_STUB_IMPL
