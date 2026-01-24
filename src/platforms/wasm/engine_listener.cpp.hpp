#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <map> // ok include

#include "engine_listener.h"
#include "platforms/wasm/active_strip_data.h"
#include "platforms/wasm/js.h"
#include "platforms/wasm/js_bindings.h"

namespace fl {

// Forward declaration of internal screenmap update function from js_bindings.cpp
// This is not part of the public API - it's an internal implementation detail
// used by the WASM-specific listener to push screenmap updates to JavaScript
void _jsSetCanvasSize(int cledcontoller_id, const ScreenMap &screenmap);

// Note: Having trouble getting this into a cpp file.
void EngineListener::Init() { Singleton<EngineListener>::instance(); }

EngineListener::EngineListener() { EngineEvents::addListener(this); }
EngineListener::~EngineListener() { EngineEvents::removeListener(this); }

void EngineListener::onEndFrame() {
    ActiveStripData &active_strips = ActiveStripData::Instance();
    jsOnFrame(active_strips);
}

void EngineListener::onStripAdded(CLEDController *strip, uint32_t num_leds) {
    // Use ActiveStripData's IdTracker for consistent ID management
    ActiveStripData &active_strips = ActiveStripData::Instance();
    int id = active_strips.getIdTracker().getOrCreateId(strip);
    jsOnStripAdded(id, num_leds);
}

void EngineListener::onCanvasUiSet(CLEDController *strip,
                                   const ScreenMap &screenmap) {
    // WASM-specific listener: Push screenmap updates to JavaScript
    // This eliminates the need for per-frame polling on the JS side
    ActiveStripData &active_strips = ActiveStripData::Instance();
    int controller_id = active_strips.getIdTracker().getOrCreateId(strip);
    _jsSetCanvasSize(controller_id, screenmap);
}

} // namespace fl

#endif // __EMSCRIPTEN__
