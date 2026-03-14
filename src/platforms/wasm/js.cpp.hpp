// IWYU pragma: private

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// ================================================================================================
// FASTLED WASM JAVASCRIPT UTILITY FUNCTIONS
// ================================================================================================
//
// This file provides WASM-specific utility functions, including an optimized delay()
// implementation that pumps fetch requests during delay periods.
// ================================================================================================

// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
// IWYU pragma: end_keep

// IWYU pragma: begin_keep
#include <string>
// IWYU pragma: end_keep
#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"  // ok include
#include "fl/stl/cstdio.h"  // ok include

#include "platforms/wasm/active_strip_data.h"
#include "platforms/wasm/engine_listener.h"
#include "fl/system/log.h"
#include "fl/stl/map.h"
#include "fl/gfx/screenmap.h"
#include "fl/stl/string.h"
#include "platforms/wasm/js.h"
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

// NOTE: All timing functions moved to CoroutineRuntimeWasm and timer.cpp.hpp
// Timer implementation (millis, micros) is in timer.cpp.hpp with EMSCRIPTEN_KEEPALIVE

namespace fl {

//////////////////////////////////////////////////////////////////////////
// NOTE: All setup/loop functionality has been moved to entry_point.cpp
// This file now provides WASM-specific utility functions including
// an optimized delay() that pumps fetch requests

} // namespace fl

#endif // FL_IS_WASM
