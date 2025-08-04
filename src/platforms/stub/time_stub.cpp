
#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "time_stub.h"
#include "fl/function.h"
#include <chrono>
#include <thread>

#if defined(FASTLED_USE_PTHREAD_DELAY) || defined(FASTLED_USE_PTHREAD_YIELD)
#include <time.h>
#include <errno.h>
#include <sched.h>
#endif

static auto start_time = std::chrono::system_clock::now();

// Global delay function override for fast testing
static fl::function<void(uint32_t)> g_delay_override;

extern "C" {

#ifndef __EMSCRIPTEN__
// STUB timing functions - excluded for WASM builds which provide their own implementations
// WASM timing functions are in src/platforms/wasm/timer.cpp and src/platforms/wasm/js.cpp

uint32_t millis() {
    auto current_time = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
}

uint32_t micros() {
    auto current_time = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time).count();
}

void delay(int ms) {
    // Use override function if set (for fast testing)
    if (g_delay_override) {
        g_delay_override(static_cast<uint32_t>(ms));
        return;
    }

    // Default delay implementation
#ifdef FASTLED_USE_PTHREAD_DELAY
    if (ms <= 0) {
        return; // nothing to wait for
    }
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    // nanosleep may be interrupted by a signal; retry until the full time has elapsed
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        // continue sleeping for the remaining time
    }
#else
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
#endif
}

void delayMicroseconds(int us) {
    if (us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
}

void yield() {
#ifdef FASTLED_USE_PTHREAD_YIELD
    // POSIX thread yield to allow other threads to run
    sched_yield();
#else
    std::this_thread::yield();
#endif
}

#endif // __EMSCRIPTEN__

} // extern "C"

// Function to set delay override (C++ linkage for test runner)
void setDelayFunction(const fl::function<void(uint32_t)>& delayFunc) {
    g_delay_override = delayFunc;
}

#endif  // FASTLED_STUB_IMPL
