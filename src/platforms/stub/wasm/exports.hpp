#pragma once



#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

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
#include <stdio.h>
#include <thread>

#include "exports.h"
#include "message_queue.h"
#include "channel_data.h"
#include "singleton.h"


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


// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

    // Replacement for 'millis' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t millis() {
        return emscripten_get_now();
    }

    // Replacement for 'micros' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t micros() {
        return millis() * 1000;
    }

    // Replacement for 'delay' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE void delay(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
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




void jsAlert(const char* msg) {
    EM_ASM_({
        // Use EM_ASM to call JavaScript directly    EM_ASM({
        var message = UTF8ToString($0);  // Convert C string to JavaScript string
        alert(message);                 // Call the JS function to show an alert
    });
}





static FastLED_ChannelData* getChannelDataPtr() {
    FastLED_ChannelData* channelData = &Singleton<FastLED_ChannelData>::instance();
    return channelData;
}


EMSCRIPTEN_BINDINGS(external_constructors) {
    emscripten::class_<FastLED_ChannelData>("FastLED_ChannelData")
        .constructor(&getChannelDataPtr, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &FastLED_ChannelData::getPixelData_Uint8);
}

void jsOnDemo() {
    EM_ASM({
        globalThis.onFastLedDemo = globalThis.onFastLedDemo || function() {
            console.log("Missing globalThis.onFastLedDemo() function");
        };
        globalThis.onFastLedDemoData = globalThis.onFastLedDemoData || new Module.FastLED_ChannelData();
        globalThis.onFastLedDemo(globalThis.onFastLedDemoData);
    });
}

#if 1
void jsOnFrame() {
    // Populate the data in C++
    //getChannelDataPtr()->update(data);
    EM_ASM_({
        globalThis.onFastLedFrame = globalThis.onFastLedFrame || function() {
            console.log("Missing globalThis.onFastLedDemo() function");
        };
        globalThis.onFastLedFrameData = globalThis.onFastLedFrameData || new Module.FastLED_ChannelData();
        globalThis.onFastLedFrame(globalThis.onFastLedFrameData);
    });
}
#else

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
#endif

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
    printf("Hello from FastLED\r\n");
    async_start_loop();
    return 0;
}

