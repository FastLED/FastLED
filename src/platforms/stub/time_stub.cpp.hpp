
// ok no namespace fl
#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "time_stub.h"
#include "fl/stl/function.h"
#include "platforms/delay_platform.h"
#include <chrono>
#include <thread>

#if defined(FASTLED_USE_PTHREAD_DELAY) || defined(FASTLED_USE_PTHREAD_YIELD)
#include <time.h>
#include <errno.h>
#include <sched.h>
#endif

static auto start_time = std::chrono::system_clock::now();  // okay std namespace

// Global delay function override for fast testing (non-static for access from platform_delay.cpp.hpp)
fl::function<void(uint32_t)> g_delay_override;

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

// Note: delay() removed - FastLED.h provides via "using fl::delay;"

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

// Check if delay override is active
bool isDelayOverrideActive(void) {
    return static_cast<bool>(g_delay_override);
}

#endif  // FASTLED_STUB_IMPL
