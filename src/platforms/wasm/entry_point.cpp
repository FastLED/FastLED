#ifdef __EMSCRIPTEN__

// ================================================================================================
// FASTLED WASM ENTRY POINT
// ================================================================================================
// 
// This file provides a proper main() entry point for WASM builds with PROXY_TO_PTHREAD support.
// When PROXY_TO_PTHREAD is enabled, Emscripten automatically moves main() to run on a pthread,
// while the browser main thread handles events and proxying.
//
// Key Features:
// - Provides main() entry point that runs on a pthread (via PROXY_TO_PTHREAD)
// - Calls setup() once during initialization  
// - Calls loop() repeatedly in main execution loop
// - Integrates with FastLED engine events and listeners
// - Maintains compatibility with existing extern_setup/extern_loop JavaScript bridges
//
// Architecture with PROXY_TO_PTHREAD:
// - Browser main thread: Handles events, DOM, and message proxying
// - pthread (this main): Runs Arduino-style setup()/loop() cycle
// - JavaScript controls timing via extern_setup()/extern_loop() calls
// - Socket proxy thread: Handled automatically by Emscripten
//
// This enables proper socket proxying while allowing JavaScript to control FastLED timing.
// ================================================================================================

#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "active_strip_data.h"
#include "engine_listener.h"
// #include "frame_buffer_manager.h"  // Temporarily commented for testing
#include "fl/async.h"
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
// MAIN ENTRY POINT (PROXY_TO_PTHREAD PATTERN)
// ================================================================================================

int main() {
    printf("FastLED WASM: Starting main() on pthread (PROXY_TO_PTHREAD mode)...\n");
    
    // In PROXY_TO_PTHREAD mode:
    // - This main() function runs on a pthread, not the browser main thread
    // - The browser main thread handles DOM events and message proxying
    // - Socket proxy functionality is handled automatically by Emscripten
    // - JavaScript controls FastLED setup/loop timing via extern_setup()/extern_loop()
    
    printf("FastLED WASM: main() pthread ready - staying alive for extern function calls...\n");
    
    // Option A: Stay alive but let JavaScript control everything
    // - Don't call setup() or loop() here - let JavaScript control timing
    // - Keep pthread alive so extern_setup()/extern_loop() can execute
    // - JavaScript uses requestAnimationFrame for proper 60fps timing
    // - Avoids race conditions between main() loop and JavaScript loop
    
    printf("FastLED WASM: main() entering async platform pump - JavaScript controls FastLED via extern functions...\n");
    
    while (true) {
        // Platform pump for async operations - update all async tasks
        fl::async_run();
        
        // Yield control to the browser more frequently for responsive async processing
        // Use 1ms sleep to maintain responsiveness while allowing other threads to work
        emscripten_sleep(1); // 1ms - frequent yielding for async pump
    }
    
    return 0; // Never reached
}

// ================================================================================================
// COMPATIBILITY EXPORTS
// ================================================================================================
// 
// These functions maintain compatibility with existing JavaScript code that expects
// extern_setup and extern_loop functions. They are the primary interface for JavaScript
// to control FastLED execution in PROXY_TO_PTHREAD mode.

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
