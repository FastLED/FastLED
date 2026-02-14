#include "is_wasm.h"

#ifdef FL_IS_WASM

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
#include "fl/stl/chrono.h"

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

// NOTE: All timing functions (millis, micros, delayMicroseconds) moved to platform_time.cpp.hpp
// Timer implementation (millis, micros) is in timer.cpp.hpp with EMSCRIPTEN_KEEPALIVE

namespace fl {

//////////////////////////////////////////////////////////////////////////
// NOTE: All setup/loop functionality has been moved to entry_point.cpp
// This file now provides WASM-specific utility functions including
// an optimized delay() that pumps fetch requests

} // namespace fl

#endif // FL_IS_WASM
