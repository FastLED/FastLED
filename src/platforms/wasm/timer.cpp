#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript TIMING BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT TIMING BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file provides critical timing functions for WASM builds. Any changes to:
// - EMSCRIPTEN_KEEPALIVE timing function signatures
// - millis(), micros(), delay(), delayMicroseconds(), yield() signatures
// - Return types or parameters
// - Function names
//
// Will BREAK JavaScript timing operations and cause SILENT RUNTIME FAILURES!
//
// Key integration points that MUST remain synchronized:
// - extern "C" uint32_t millis()
// - extern "C" uint32_t micros()
// - extern "C" void delay(int ms)
// - extern "C" void delayMicroseconds(int micros)
// - extern "C" void yield()
// - JavaScript Module.cwrap() calls for timing functions
//
// Before making ANY changes:
// 1. Understand this affects ALL timing-dependent animations and sketches
// 2. Test with real WASM builds that use delays and timing
// 3. Verify timing accuracy remains consistent
// 4. Check that animations still run at correct speeds
//
// ⚠️⚠️⚠️ REMEMBER: Timing errors break ALL animations and effects! ⚠️⚠️⚠️

#include "fl/stdint.h"
#include <thread>

#include <emscripten.h>
#include <emscripten/html5.h>

namespace {
// We are just going to get the time since the app started. Getting the
// global real time is not guaranteed to be accurate to any UTC time via the
// browser. In the future, we will want to make an allowance for clock
// synchronization across multiple devices. However, that might be difficult
// since millis() is sort of understood to be monotonically increasing, except
// after a rollover after 50 days. We will need to think about this more in the
// future to make multiple devices sync up to just one clock. Otherwise this may
// just be something the user has to do themselves.
double gStartTime = emscripten_performance_now();
double get_time_since_epoch() { return emscripten_get_now() - gStartTime; }
} // namespace

// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t millis() {
    return uint32_t(get_time_since_epoch());
}

// Replacement for 'micros' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t micros() {
    uint64_t out = uint64_t(get_time_since_epoch() * 1000);
    return uint32_t(out & 0xFFFFFFFF);
}

// Replacement for 'delay' in WebAssembly context
EMSCRIPTEN_KEEPALIVE void delay(int ms) {
    // Keep in mind this is NOT ASYNC as of 2024-12, and will block the main
    // thread. If you do delay(10000) it will result in a 10 second block on the
    // main thread and the browser may kill your app. Using async sleep was
    // looked at but the emscripten documentation says it will add a TON of
    // overhead and is not recommended unless compiling with -O3, which we do
    // NOT want to do since emscripten builds must be as fast as possible for
    // the quick build path and must be as small as possible to reduce web
    // compiler upload times back to the client.
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

EMSCRIPTEN_KEEPALIVE void delayMicroseconds(int micros) {
    // Keep in mind this is NOT ASYNC as of 2024-12, and will block the main
    // thread. If you do delay(10000) it will result in a 10 second block on the
    // main thread and the browser may kill your app. Using async sleep was
    // looked at but the emscripten documentation says it will add a TON of
    // overhead and is not recommended unless compiling with -O3, which we do
    // NOT want to do since emscripten builds must be as fast as possible for
    // the quick build path and must be as small as possible to reduce web
    // compiler upload times back to the client.
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
}

// Replacement for 'yield' in WebAssembly context
EMSCRIPTEN_KEEPALIVE void yield() {
    // Use emscripten_yield to allow the browser to perform other tasks
    delay(0);
}
}

#endif // __EMSCRIPTEN__
