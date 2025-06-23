#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript SETUP/LOOP BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT SETUP/LOOP BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file exposes the Arduino-style setup() and loop() functions to JavaScript.
// Any changes to:
// - extern "C" extern_setup() function signature
// - extern "C" extern_loop() function signature  
// - EMSCRIPTEN_KEEPALIVE function signatures
// - Return types or parameters
//
// Will BREAK JavaScript sketch execution and cause SILENT RUNTIME FAILURES!
//
// Key integration points that MUST remain synchronized:
// - extern "C" int extern_setup()
// - extern "C" int extern_loop()
// - JavaScript Module.cwrap() calls for setup/loop
// - Arduino sketch execution lifecycle
//
// Before making ANY changes:
// 1. Understand this controls the main sketch execution loop
// 2. Test with real WASM builds that run Arduino sketches
// 3. Verify setup() and loop() are called correctly from JavaScript
// 4. Check that engine events still fire properly
//
// ⚠️⚠️⚠️ REMEMBER: These functions control the main application lifecycle! ⚠️⚠️⚠️

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

struct EndFrameListener : public fl::EngineEvents::Listener {
    EndFrameListener() = default;
    void onEndFrame() override {
        // This is called at the end of the frame.
        // We can do some cleanup here if needed.
        // For now, we will just call the JavaScript function.
        mEndFrameHappened = true;
    }
    bool mEndFrameHappened = false;

    bool endFrameHappened() {
        bool result = mEndFrameHappened;
        mEndFrameHappened = false;
        return result;
    }
};

EndFrameListener gEndFrameListener;

inline void setup_once() {
    static bool g_setup_called = false;
    if (g_setup_called) {
        return;
    }
    EngineListener::Init();
    EngineEvents::addListener(&gEndFrameListener);
    g_setup_called = true;
    setup();
}

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup() {
    setup_once();
    return 0;
}

EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop() {
    setup_once();
    // fastled_resume_timer();
    fl::EngineEvents::onPlatformPreLoop();
    loop();

    if (!gEndFrameListener.endFrameHappened()) {
        // UI needs to pump the event loop.
        // This happens if the sketch fails to call FastLED.show().
        // This is a problem because the UI will not update, so we manually pump
        // it here.
        fl::EngineEvents::onEndFrame();
    }
    // fastled_pause_timer();
    return 0;
}

} // namespace fl

#endif // __EMSCRIPTEN__
