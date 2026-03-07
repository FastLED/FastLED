// IWYU pragma: private

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// ================================================================================================
// FASTLED WASM ENTRY POINT
// ================================================================================================
//
// WASM runs on a Web Worker — no pthreads needed. JavaScript controls timing
// by calling extern_setup() once, then extern_loop() on each animation frame.
// main() just returns; the runtime stays alive via -sEXIT_RUNTIME=0.

// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/emscripten.h>
// IWYU pragma: end_keep

#include "platforms/wasm/active_strip_data.h"
#include "platforms/wasm/engine_listener.h"
#include "fl/dbg.h"
#include "fl/stl/async.h"
#include "fl/warn.h"
// IWYU pragma: begin_keep
#include "fl/stl/cstdio.h"  // ok include
// IWYU pragma: end_keep

extern void setup();
extern void loop();

namespace fl {

struct EndFrameListener : public fl::EngineEvents::Listener {
    EndFrameListener() = default;
    void onEndFrame() override { mEndFrameHappened = true; }
    bool endFrameHappened() {
        bool result = mEndFrameHappened;
        mEndFrameHappened = false;
        return result;
    }
private:
    bool mEndFrameHappened = false;
};

static EndFrameListener gEndFrameListener;

void fastled_setup_once() {
    static bool setup_called = false;
    if (setup_called) return;
    EngineListener::Init();
    EngineEvents::addListener(&gEndFrameListener);
    setup();
    setup_called = true;
}

void fastled_loop_once() {
    fastled_setup_once();
    fl::EngineEvents::onPlatformPreLoop();
    fl::async_run();
    loop();
    if (!gEndFrameListener.endFrameHappened()) {
        fl::EngineEvents::onEndFrame();
    }
}

} // namespace fl

int main() {
    // main() returns immediately. Runtime stays alive (-sEXIT_RUNTIME=0).
    // JavaScript calls extern_setup()/extern_loop() to drive the sketch.
    return 0;
}

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

#endif // FL_IS_WASM
