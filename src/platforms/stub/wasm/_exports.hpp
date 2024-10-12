#pragma once

/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <iostream> // ok include
#include <deque>  // ok include
#include <string> // ok include

#include "_timer.hpp"
#include "message_queue.h"


extern void setup();
extern void loop();

// Frame time for 60 fps.
#define SIXTY_FPS 16
static bool g_setup_called = false;

void setup_once() {
    if (g_setup_called) {
        return;
    }
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
    //fastled_resume_timer();
    loop();
    //fastled_pause_timer();
    return 0;
}

#if 0

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData);

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData) {
    extern_loop();
    emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
    return true;
}
#endif

void interval_loop(void* userData) {
    extern_loop();
}



EMSCRIPTEN_KEEPALIVE extern "C" void async_start_loop() {
  // Receives a function to call and some user data to provide it.
  //emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
  emscripten_set_interval(interval_loop, SIXTY_FPS, nullptr);
}




// Function to invoke the JavaScript function to run a script
void invokeScriptFromJS(const char* script) {
    // Use EM_ASM to call JavaScript directly
    //EM_ASM({
    //    var script = UTF8ToString($0);  // Convert C string to JavaScript string
    //    emscripten_run_script(script);              // Call the JS function to run the script
    //}, scriptPath);
    emscripten_run_script(script);
}

void jsAlert(const char* msg) {
    EM_ASM_({
        // Use EM_ASM to call JavaScript directly    EM_ASM({
        var message = UTF8ToString($0);  // Convert C string to JavaScript string
        alert(message);                 // Call the JS function to show an alert
    });
}


class FastLED_ChannelData {
public:
    static FastLED_ChannelData* passThrough(FastLED_ChannelData* ptr) { return ptr; }
    static FastLED_ChannelData* createC() { return new FastLED_ChannelData(); }

    FastLED_ChannelData() {}
    emscripten::val getPixelData_Uint8(int stripIndex) {
        const VectorUint8* stripData = getStripData(stripIndex);
        if (!stripData) {
            return emscripten::val::null();
        }
        return emscripten::val(emscripten::typed_memory_view(stripData->size(), stripData->data()));
    }
    
private:
    typedef std::vector<uint8_t> VectorUint8;
    typedef std::pair<int, VectorUint8> StripData;  // strip index -> data
    typedef std::unique_ptr<StripData> StripDataPtr;
    std::vector<StripDataPtr> mStripData;

    const VectorUint8* getStripData(int stripIndex) const {
        // search through the vector for the strip index
        for (const auto& stripDataPtr : mStripData) {
            if (stripDataPtr->first == stripIndex) {
                return &stripDataPtr->second;
            }
        }
        return nullptr;
    }
};

EMSCRIPTEN_BINDINGS(better_smart_pointers) {
    emscripten::class_<FastLED_ChannelData>("FastLED_ChannelData")
        .smart_ptr_constructor("FastLED_ChannelData", &std::make_shared<FastLED_ChannelData>);
}

#if 0
void jsOnFrame(const char* message /*ignored for now*/) {
    // Use EM_ASM to call JavaScript directly
    auto ptr = std::make_shared<FastLED_ChannelData>();
    EM_ASM_({
        globalThis.onFastLedFrame = globalThis.onFastLedFrame || function(jsonStr) {
            console.log("Missing globalThis.onFastLedFrame(jsonStr) function");
        };

        emscripten::var message = ptr;  // Convert C string to JavaScript string
        //console.log(message);            // Log the message to the console
        //globalThis.postMessage({ type: 'message', message: message }); // Send the message to the main thread
        globalThis.onFastLedFrame(message);

    }, ptr.get());
}
#endif

void jsOnFrame(const char* message) {
    // Use EM_ASM to call JavaScript directly
    EM_ASM_({
        globalThis.onFastLedFrame = globalThis.onFastLedFrame || function(jsonStr) {
            console.log("Missing globalThis.onFastLedFrame(jsonStr) function");
        };
        var message = UTF8ToString($0);  // Convert C string to JavaScript string
        //console.log(message);            // Log the message to the console
        //globalThis.postMessage({ type: 'message', message: message }); // Send the message to the main thread
        globalThis.onFastLedFrame(message);

    }, message);
}

void jsSetCanvasSize(int width, int height) {
    char jsonStr[1024];
    snprintf(jsonStr, sizeof(jsonStr), "[{\"width\":%d,\"height\":%d}]", width, height);
    EM_ASM_({
        globalThis.onFastLedSetCanvasSize = globalThis.onFastLedSetCanvasSize || function(jsonStr) {
            console.log("Missing globalThis.onFastLedSetCanvasSize(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0);  // Convert C string to JavaScript string
        globalThis.onFastLedSetCanvasSize(jsonStr);
    }, jsonStr);
}

MessageQueue js_message_queue;  // hack - this needs to not be inlined into the hpp file!

EMSCRIPTEN_KEEPALIVE extern "C" bool postMessage(const char* jstStr) {
    // post message to the message queue.
    return js_message_queue.message_push_back(jstStr);
}

EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    const char* scriptPath = "/path/to/your/script.sh"; // Change to your script path
    invokeScriptFromJS(scriptPath);
    printf("Hello from FastLED\r\n");
    async_start_loop();
    return 0;
}
