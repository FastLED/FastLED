#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include <memory>
#include <stdint.h>
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
#include "ui/ui_internal.h"

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
