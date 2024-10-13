#pragma once


#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "platforms/stub/wasm/channel_data.h"
#include "engine_events.h"

static ChannelData* getChannelDataPtr() {
    ChannelData* channelData = &Singleton<ChannelData>::instance();
    return channelData;
}

EMSCRIPTEN_BINDINGS(external_constructors) {
    emscripten::class_<ChannelData>("ChannelData")
        .constructor(&getChannelDataPtr, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &ChannelData::getPixelData_Uint8)
        .function("getFirstPixelData_Uint8", &ChannelData::getFirstPixelData_Uint8)
        .function("getNthPixelStripData_Uint8", &ChannelData::getNthPixelStripData_Uint8);
}

void jsOnFrame() {
    EM_ASM_({
        globalThis.onFastLedFrame = globalThis.onFastLedFrame || function() {
            console.log("Missing globalThis.onFastLedDemo() function");
        };
        globalThis.onFastLedFrameData = globalThis.onFastLedFrameData || new Module.ChannelData();
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
