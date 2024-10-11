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
void invokeScriptFromJS(const char* scriptPath) {
    // Use EM_ASM to call JavaScript directly
    EM_ASM({
        var script = UTF8ToString($0);  // Convert C string to JavaScript string
        runScript(script);              // Call the JS function to run the script
    }, scriptPath);
}

EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    const char* scriptPath = "/path/to/your/script.sh"; // Change to your script path
    invokeScriptFromJS(scriptPath);
    printf("Hello from FastLED\r\n");
    async_start_loop();
    return 0;
}
