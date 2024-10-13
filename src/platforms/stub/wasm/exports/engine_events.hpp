#pragma once

#include <map>

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "platforms/stub/wasm/active_strip_data.h"
#include "engine_events.h"
#include "platforms/stub/wasm/strip_id_map.h"


#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class CLEDController;

static ActiveStripData* getActiveStripDataPtr() {
    ActiveStripData* instance = &Singleton<ActiveStripData>::instance();
    return instance;
}

EMSCRIPTEN_BINDINGS(external_constructors) {
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
        int id = StripIdMap::add(strip);
        jsOnStripAdded(id, num_leds);
    }
    void jsOnFrame() {
        EM_ASM_({
            globalThis.onFastLedFrame = globalThis.onFastLedFrame || function() {
                console.log("Missing globalThis.onFastLedDemo() function");
            };
            globalThis.onFastLedFrameData = globalThis.onFastLedFrameData || new Module.ActiveStripData();
            globalThis.onFastLedFrame(globalThis.onFastLedFrameData);
        });
    }

    void jsOnStripAdded(uintptr_t strip, uint32_t num_leds) {
        EM_ASM_({
            globalThis.onFastLedStripAdded = globalThis.onFastLedStripAdded || function() {
                console.log("Missing globalThis.onFastLedStripAdded(id, length) function");
                console.log("Added strip id: " + arguments[0] + " with length: " + arguments[1]);
            };
            globalThis.onFastLedStripAdded($0, $1);
        }, strip, num_leds);
    }

    EngineListener() {}
};

void EngineListener::Init() {
    Singleton<EngineListener>::instance();
}

FASTLED_NAMESPACE_END
