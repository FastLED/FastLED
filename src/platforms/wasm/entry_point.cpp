#ifdef __EMSCRIPTEN__

// ================================================================================================
// FASTLED WASM ENTRY POINT
// ================================================================================================
// 
// This file provides a proper main() entry point for WASM builds, allowing us to remove the
// --no-entry linker flag. This follows the standard Arduino setup()/loop() pattern but runs
// as a proper C program with a main() function.
//
// Key Features:
// - Provides main() entry point for WASM module
// - Calls setup() once during initialization  
// - Calls loop() repeatedly in main execution loop
// - Integrates with FastLED engine events and listeners
// - Maintains compatibility with existing extern_setup/extern_loop JavaScript bridges
//
// Architecture:
// - main() serves as the primary entry point
// - setup() is called once for initialization
// - loop() is called repeatedly for the main execution cycle
// - Engine events are properly managed throughout the lifecycle
//
// This replaces the need for --no-entry and manual JavaScript-driven execution cycles.
// ================================================================================================

#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "active_strip_data.h"
#include "engine_listener.h"
// #include "frame_buffer_manager.h"  // Temporarily commented for testing
#include "fl/dbg.h"
#include "fl/namespace.h"
#include "fl/warn.h"
#include <stdio.h>

// Forward declarations for Arduino-style setup/loop functions
// These will be provided by the user's sketch
extern void setup();
extern void loop();

namespace fl {

// EndFrameListener to track frame completion events
struct EndFrameListener : public fl::EngineEvents::Listener {
    EndFrameListener() = default;
    
    void onEndFrame() override {
        mEndFrameHappened = true;
    }
    
    bool endFrameHappened() {
        bool result = mEndFrameHappened;
        mEndFrameHappened = false;
        return result;
    }
    
private:
    bool mEndFrameHappened = false;
};

// Global frame listener instance
static EndFrameListener gEndFrameListener;

// One-time initialization function
void fastled_setup_once() {
    static bool setup_called = false;
    if (setup_called) {
        return;
    }
    
    printf("FastLED WASM: Initializing engine and listeners...\n");
    
    // Initialize engine listener and events
    EngineListener::Init();
    EngineEvents::addListener(&gEndFrameListener);
    
    // Note: Thread-safe frame buffer manager not needed in WASM single-threaded environment
    // Using existing ActiveStripData system for frame data management
    
    printf("FastLED WASM: Calling user setup()...\n");
    
    // Call user's setup function
    setup();
    
    setup_called = true;
    printf("FastLED WASM: Setup complete.\n");
}

// Single loop iteration function  
void fastled_loop_once() {
    // Ensure setup has been called
    fastled_setup_once();
    
    // Call pre-loop engine events
    fl::EngineEvents::onPlatformPreLoop();
    
    // Call user's loop function
    loop();
    
    // Check if frame ended naturally (via FastLED.show())
    if (!gEndFrameListener.endFrameHappened()) {
        // Frame didn't end naturally - manually trigger end frame event
        // This handles cases where sketches don't call FastLED.show()
        fl::EngineEvents::onEndFrame();
    }
}

} // namespace fl

// ================================================================================================
// MAIN ENTRY POINT
// ================================================================================================

int main() {
    printf("FastLED WASM: Starting main() entry point...\n");
    
    // Perform one-time setup
    fl::fastled_setup_once();
    
    printf("FastLED WASM: Entering main loop...\n");
    
    // Main execution loop - runs indefinitely
    while (true) {
        fl::fastled_loop_once();
        
        // Yield control back to browser event loop
        // This prevents blocking the browser and allows other events to process
        emscripten_sleep(0);
    }
    
    // This should never be reached in a typical WASM environment
    printf("FastLED WASM: Main loop exited (unexpected).\n");
    return 0;
}

// ================================================================================================
// COMPATIBILITY EXPORTS
// ================================================================================================
// 
// These functions maintain compatibility with existing JavaScript code that expects
// extern_setup and extern_loop functions. They can be called from JavaScript if needed,
// but the main() function above handles the primary execution flow.

extern "C" {

EMSCRIPTEN_KEEPALIVE int extern_setup() {
    fl::fastled_setup_once();
    return 0;
}

EMSCRIPTEN_KEEPALIVE int extern_loop() {
    fl::fastled_loop_once();
    return 0;
}

}

#endif // __EMSCRIPTEN__ 
