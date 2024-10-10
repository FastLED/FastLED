// Wasm build of FastLED.


// This code and index.html are **strongly coupled**. You must make sure that
// if you are editing this file that you are also changing index.html to match.
// Otherwise you will break the build.



// This code will generate a js file that can be run with the index.html file
// provided in the repo. The index.html file will load the js file an invoke
// start on it.


#include "platform_compatibility.h"


#include "FastLED.h"

// This is the equivalent of an ino file.
#include "noise_palette.hpp"


/// Begin compatibility layer for FastLED platform. WebAssembly edition.


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
    fastled_resume_timer();
    loop();
    fastled_pause_timer();
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


EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    printf("Hello from FastLED\r\n");
    async_start_loop();
    return 0;
}
