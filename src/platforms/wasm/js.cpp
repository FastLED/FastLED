#ifdef __EMSCRIPTEN__

// ================================================================================================
// FASTLED WASM JAVASCRIPT UTILITY FUNCTIONS
// ================================================================================================
//
// This file is reserved for future WASM-specific JavaScript utility functions.
// The main entry point and setup/loop functionality has been moved to entry_point.cpp.
//
// Previously this file contained:
// - extern_setup() and extern_loop() functions (now in entry_point.cpp)
// - EndFrameListener implementation (now in entry_point.cpp) 
// - setup_once() function (now in entry_point.cpp as fastled_setup_once())
//
// Current purpose:
// - Reserved for future WASM-specific utility functions
// - JavaScript interop helper functions (if needed)
// - WASM-specific debugging or diagnostic functions
//
// Note: The main application lifecycle is now handled by the main() function
// in entry_point.cpp, which provides a proper C program entry point.
// ================================================================================================

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include <memory>
#include "fl/stdint.h"
#include <stdio.h>
#include <string>

#include "active_strip_data.h"
#include "engine_listener.h"
#include "fl/dbg.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/str.h"
#include "js.h"
#include "platforms/shared/ui/json/ui_internal.h"

// extern setup and loop which will be supplied by the sketch.
extern void setup();
extern void loop();

namespace fl {

//////////////////////////////////////////////////////////////////////////
// NOTE: All setup/loop functionality has been moved to entry_point.cpp
// This file is now reserved for future WASM-specific utility functions

} // namespace fl

#endif // __EMSCRIPTEN__
