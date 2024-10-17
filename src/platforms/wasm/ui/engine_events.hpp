#pragma once

#include <map>

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "platforms/wasm/js.h"

#include "platforms/wasm/active_strip_data.h"
#include "engine_events.h"
#include "platforms/wasm/strip_id_map.h"
#include "platforms/wasm/js.h"


#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class CLEDController;

static ActiveStripData* getActiveStripDataPtr() {
    ActiveStripData* instance = &Singleton<ActiveStripData>::instance();
    return instance;
}

EMSCRIPTEN_BINDINGS(engine_events_constructors) {
    emscripten::class_<ActiveStripData>("ActiveStripData")
        .constructor(&getActiveStripDataPtr, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &ActiveStripData::getPixelData_Uint8)
        .function("getFirstPixelData_Uint8", &ActiveStripData::getFirstPixelData_Uint8)
        .function("getNthPixelStripData_Uint8", &ActiveStripData::getNthPixelStripData_Uint8);
}


class EngineListener: public EngineEvents::Listener {
public:
    friend class Singleton<EngineListener>;
    static void Init();

private:
    void onEndFrame() override {
        jsOnFrame();
    }
    void onStripAdded(CLEDController* strip, uint32_t num_leds) override {
        int id = StripIdMap::addOrGetId(strip);
        jsOnStripAdded(id, num_leds);
    }

    void onCanvasUiSet(CLEDController* strip, const XYMap& xymap) override {
        int controller_id = StripIdMap::addOrGetId(strip);
        jsSetCanvasSize(controller_id, xymap);
    }


    EngineListener() {
        EngineEvents::addListener(this);
    }
    ~EngineListener() {
        EngineEvents::removeListener(this);
    }
};

void EngineListener::Init() {
    Singleton<EngineListener>::instance();
}

FASTLED_NAMESPACE_END
