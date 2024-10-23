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


class EngineListener: public EngineEvents::Listener {
public:
    friend class Singleton<EngineListener>;
    static void Init();

private:
    void onEndFrame() override {
        ActiveStripData& active_strips = ActiveStripData::Instance();
        jsOnFrame(active_strips);
    }
    void onStripAdded(CLEDController* strip, uint32_t num_leds) override {
        int id = StripIdMap::addOrGetId(strip);
        jsOnStripAdded(id, num_leds);
    }

    void onCanvasUiSet(CLEDController* strip, const ScreenMap& screenmap) override {
        int controller_id = StripIdMap::addOrGetId(strip);
        jsSetCanvasSize(controller_id, screenmap);
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
