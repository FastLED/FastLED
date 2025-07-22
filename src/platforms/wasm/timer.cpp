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
#include <cmath>  // For fmod function

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
double gStartTime = emscripten_get_now();  // Use emscripten_get_now() for consistency

double get_time_since_epoch() {
    double now = emscripten_get_now();
    double elapsed = now - gStartTime;
    
    // Debug output to track the timing issue
    //FASTLED_WARN("gStartTime: " << gStartTime);
    //FASTLED_WARN("now: " << now);
    //FASTLED_WARN("elapsed: " << elapsed);
    
    // Ensure we return a reasonable positive elapsed time
    if (elapsed < 0 || elapsed == 0xffffffff) {
        FASTLED_WARN("WARNING: Negative elapsed time detected, resetting start time");
        gStartTime = now;
        elapsed = 0;
    }
    
    return elapsed;
}
} // namespace

// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t millis() {
    double elapsed_ms = get_time_since_epoch();
    
    // Handle potential overflow - Arduino millis() wraps around every ~49.7 days
    // This matches Arduino behavior where millis() overflows back to 0
    if (elapsed_ms >= UINT32_MAX) {
        elapsed_ms = fmod(elapsed_ms, UINT32_MAX);
    }
    
    uint32_t result = uint32_t(elapsed_ms);
    return result;
}

// Replacement for 'micros' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t micros() {
    double elapsed_ms = get_time_since_epoch();
    double elapsed_micros = elapsed_ms * 1000.0;
    
    // Handle potential overflow - Arduino micros() wraps around every ~71.6 minutes
    // This matches Arduino behavior where micros() overflows back to 0
    if (elapsed_micros >= UINT32_MAX) {
        elapsed_micros = fmod(elapsed_micros, UINT32_MAX);
    }
    
    uint32_t result = uint32_t(elapsed_micros);
    return result;
}

// NOTE: delay() and delayMicroseconds() are implemented in js.cpp
// with async task pumping for better performance during delays

// Replacement for 'yield' in WebAssembly context
EMSCRIPTEN_KEEPALIVE void yield() {
    // Use emscripten_yield to allow the browser to perform other tasks
    delay(0);
}
}

#endif // __EMSCRIPTEN__
