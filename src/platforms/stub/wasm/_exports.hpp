#pragma once

/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "_timer.hpp"

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

void jsAlert() {
    // Use EM_ASM to call JavaScript directly
    EM_ASM(alert('hai'));
}

void jsOutput(const char* message) {
    // Use EM_ASM to call JavaScript directly
    EM_ASM_({
        globalThis.fastledOnFrame = globalThis.onFastLedFrame || function(jsonStr) {
            console.log("Missing globalThis.onFastLedFrame() function");
        };
        var message = UTF8ToString($0);  // Convert C string to JavaScript string
        //console.log(message);            // Log the message to the console
        //globalThis.postMessage({ type: 'message', message: message }); // Send the message to the main thread
        globalThis.fastledOnFrame(message);

    }, message);
}

EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    const char* scriptPath = "/path/to/your/script.sh"; // Change to your script path
    invokeScriptFromJS(scriptPath);
    printf("Hello from FastLED\r\n");
    async_start_loop();
    return 0;
}
