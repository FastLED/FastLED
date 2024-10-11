#pragma once

/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
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

#include <emscripten.h>
#include <iostream>




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
    EM_ASM_({
        globalThis.onFastLedSetCanvasSize = globalThis.onFastLedSetCanvasSize || function(jsonData) {
            console.log("Missing globalThis.onFastLedSetCanvasSize(jsonData) function");
        };
        var jsonData = [{ width: $0, height: $1 }];
        globalThis.onFastLedSetCanvasSize(jsonData);
    }, width, height);
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
