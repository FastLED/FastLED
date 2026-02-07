/// @file init_wasm.cpp
/// @brief WebAssembly platform initialization implementation
///
/// This file provides the implementation of platform initialization for WASM.
/// It initializes the engine listener system for JavaScript runtime integration.

#include "fl/compiler_control.h"
#include "is_wasm.h"

#ifdef FL_IS_WASM

#include "platforms/wasm/init_wasm.h"
#include "fl/dbg.h"

// Include engine listener header
#include "platforms/wasm/engine_listener.h"

// Include channel engine initialization
#include "platforms/wasm/init_channel_engine.h"

namespace fl {
namespace platforms {

/// @brief Initialize WebAssembly platform
///
/// Performs one-time initialization of WASM-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("WASM: Platform initialization starting");

    // Initialize engine listener system
    // This connects FastLED engine events to the JavaScript runtime
    EngineListener::Init();

    // Initialize channel engines for WASM platform
    // This registers the stub engine with ChannelBusManager
    platforms::initChannelEngines();

    initialized = true;
    FL_DBG("WASM: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_WASM
