// This code will generate a js file that can be run with the index.html file
// provided in the repo. The index.html file will load the js file an invoke
// start on it.



#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


// BEGIN COMPATIBILITY PREABMLE


extern "C" {
    // Replacement for 'micros' in WebAssembly context
    uint32_t micros() {
        return emscripten_get_now() * 1000;  // Emscripten provides milliseconds, multiply to get microseconds
    }

    // Replacement for 'millis' in WebAssembly context
    uint32_t millis() {
        return emscripten_get_now();  // Emscripten returns time in milliseconds
    }

    // Replacement for 'delay' in WebAssembly context
    void delay(int ms) {
        emscripten_sleep(ms);  // Non-blocking sleep for the specified time in milliseconds
    }
}


// END COMPATIBILITY PREABMLE

#include "FastLED.h"
#include "noise_palette.hpp"





// This is a very early preview of a the wasm build of FastLED.
// Right now this demo only works in node.js, but the goal is to make it work in the browser, too.
// 
// To do this demo you are going to grab a copy of the emscripten build. Just look at the wasm builds on the front
// page of fastled and you'll find it in one of the build steps. Small too.
// Then make sure you have node installed.
//
// Open up node.
//  $ node
//  > const foo = await import('./fastled.js')
//  > foo.default()
//  > console.log(await foo.default()[0])
//  ---> prints out "Hello from FastLED"
//  ## On node using require(...)
//  Or alternatively, you can run this:
//  > let fastled = require('./fastled');
//  > fastled();


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
    printf("FastLED extern_loop.\r\n");
    setup_once();
    loop();
    return 0;
}

// emscripten_set_interval(void (*cb)(void *userData), double intervalMsecs, void *userData)

// define request animation frame

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData);

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData) {
    extern_loop();
    emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
    return true;
}

void interval_loop(void* userData) {
    extern_loop();
}


EMSCRIPTEN_KEEPALIVE extern "C" void start_loop() {
  // Receives a function to call and some user data to provide it.
  //emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
  emscripten_set_interval(interval_loop, 1000, nullptr);
}


EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    printf("Hello from FastLED\r\n");
    /*
    setup();
    while(true) {
        loop();
    }
    */
    start_loop();
    return 0;
}
