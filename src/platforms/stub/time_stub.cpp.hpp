
// ok no namespace fl
#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "time_stub.h"
#include "fl/stl/function.h"

#if defined(FASTLED_USE_PTHREAD_YIELD)
#include <sched.h>
#endif

#include "platforms/wasm/is_wasm.h"
#ifndef FL_IS_WASM
#include <thread>
#endif

// Global delay function override for fast testing (non-static for access from platform_time.cpp.hpp)
fl::function<void(fl::u32)> g_delay_override;

extern "C" {

#if !defined(FL_IS_WASM) && !defined(FASTLED_NO_ARDUINO_STUBS)
// STUB timing functions - excluded for WASM builds which provide their own implementations
// Also excluded when FASTLED_NO_ARDUINO_STUBS is defined (for compatibility with ArduinoFake, etc.)

// Global millis() and micros() forward to fl:: layer (which handles time injection in tests)
// These are needed for Arduino API compatibility
fl::u32 millis() {
    return fl::millis();
}

fl::u32 micros() {
    return fl::micros();
}

void yield() {
#ifdef FASTLED_USE_PTHREAD_YIELD
    // POSIX thread yield to allow other threads to run
    sched_yield();
#else
    std::this_thread::yield();  // okay std namespace
#endif
}

#endif // !FL_IS_WASM && !FASTLED_NO_ARDUINO_STUBS

} // extern "C"

// Function to set delay override (C++ linkage for test runner)
void setDelayFunction(const fl::function<void(fl::u32)>& delayFunc) {
    g_delay_override = delayFunc;
}

// Check if delay override is active
bool isDelayOverrideActive(void) {
    return static_cast<bool>(g_delay_override);
}

#endif  // FASTLED_STUB_IMPL
