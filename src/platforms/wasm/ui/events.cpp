#ifdef __EMSCRIPTEN__



#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>


#include <map>

#include "events.h"
#include "namespace.h"

#include "platforms/wasm/js.h"

FASTLED_NAMESPACE_BEGIN

// Note: Having trouble getting this into a cpp file.
void EngineListener::Init() {
    Singleton<EngineListener>::instance();
}

EngineListener::EngineListener() {
    EngineEvents::addListener(this);
}
EngineListener::~EngineListener() {
    EngineEvents::removeListener(this);
}

void EngineListener::onEndFrame() {
    ActiveStripData& active_strips = ActiveStripData::Instance();
    jsOnFrame(active_strips);
}

void EngineListener::onStripAdded(CLEDController* strip, uint32_t num_leds) {
    int id = StripIdMap::addOrGetId(strip);
    jsOnStripAdded(id, num_leds);
}

void EngineListener::onCanvasUiSet(CLEDController* strip, const ScreenMap& screenmap) {
    int controller_id = StripIdMap::addOrGetId(strip);
    jsSetCanvasSize(controller_id, screenmap);
}


FASTLED_NAMESPACE_END

#endif  // __EMSCRIPTEN__
