#pragma once


#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "platforms/stub/wasm/active_strip_data.h"
#include "engine_events.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

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

void jsOnFrame() {
    EM_ASM_({
        globalThis.onFastLedFrame = globalThis.onFastLedFrame || function() {
            console.log("Missing globalThis.onFastLedDemo() function");
        };
        globalThis.onFastLedFrameData = globalThis.onFastLedFrameData || new Module.ActiveStripData();
        globalThis.onFastLedFrame(globalThis.onFastLedFrameData);
    });
}


class OnEndFrameListener: public EngineEvents::Listener {
public:
    friend class Singleton<OnEndFrameListener>;
    static void Init();
    void onEndFrame() override {
        jsOnFrame();
    }
private:
    OnEndFrameListener() {}
};

void OnEndFrameListener::Init() {
    Singleton<OnEndFrameListener>::instance();
}

FASTLED_NAMESPACE_END
