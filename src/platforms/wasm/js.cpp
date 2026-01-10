#ifdef __EMSCRIPTEN__

// ================================================================================================
// FASTLED WASM JAVASCRIPT UTILITY FUNCTIONS
// ================================================================================================
//
// This file provides WASM-specific utility functions, including an optimized delay()
// implementation that pumps fetch requests during delay periods.
// ================================================================================================

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include <memory> // ok include
#include "fl/stl/stdint.h"
#include <stdio.h> // ok include
#include <string>

#include "active_strip_data.h"
#include "engine_listener.h"
#include "fl/dbg.h"
#include "fl/stl/map.h"
#include "fl/screenmap.h"
#include "fl/str.h"
#include "js.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/stl/time.h"

// extern setup and loop which will be supplied by the sketch.
extern void setup();
extern void loop();

// Forward declaration for async update function
namespace fl {
    void async_run();
}

//////////////////////////////////////////////////////////////////////////
// WASM-SPECIFIC UTILITY FUNCTIONS
//////////////////////////////////////////////////////////////////////////

extern "C" {

/// @brief Custom delay implementation for WASM that pumps async tasks
/// @param ms Number of milliseconds to delay
/// 
/// This optimized delay() breaks the delay period into 1ms chunks and pumps
/// all async tasks (fetch, timers, etc.) during each interval, making delay 
/// time useful for processing async operations instead of just blocking.
void delay(int ms) {
    if (ms <= 0) {
        return;
    }

    uint32_t end = fl::millis() + ms;

    // Break delay into 1ms chunks and pump all async tasks
    while (fl::millis() < end) {
        // Update all async tasks (fetch, timers, etc.) during delay
        fl::async_run();

        if (fl::millis() >= end) {
            break;
        }

        // In worker thread mode (PROXY_TO_PTHREAD), busy-wait is acceptable
        // since we're not blocking the browser's main thread or UI
    }
}

/// @brief Microsecond delay implementation for WASM  
/// @param micros Number of microseconds to delay
///
/// For microsecond delays, we use Emscripten's busywait since pumping
/// fetch requests every microsecond would be too expensive.
void delayMicroseconds(int micros) {
    if (micros <= 0) {
        return;
    }
    
    // For microsecond precision, use busy wait
    // Converting microseconds to milliseconds for emscripten_sleep would lose precision
    double start = emscripten_get_now();
    double target = start + (micros / 1000.0); // Convert to milliseconds
    
    while (emscripten_get_now() < target) {
        // Busy wait for microsecond precision
        // No fetch pumping here as it would be too expensive
    }
}

// NOTE: millis() and micros() functions are defined in timer.cpp with EMSCRIPTEN_KEEPALIVE
// to avoid duplicate definitions in unified builds

} // extern "C"

namespace fl {

//////////////////////////////////////////////////////////////////////////
// NOTE: All setup/loop functionality has been moved to entry_point.cpp
// This file now provides WASM-specific utility functions including
// an optimized delay() that pumps fetch requests

} // namespace fl

#endif // __EMSCRIPTEN__
