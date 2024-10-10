#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


#include "FastLED.h"


namespace {

bool g_setup_called = false;
int g_loop_job_id = -1;
int g_frame_time = 1000 / 60;
}  // namespace hack



void setup() {
   printf("FastLED setup ran.\r\n");
   if (g_setup_called) {
       return;
   }
}

void loop() {
   printf("FastLED loop ran.\r\n");
}

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


void setup_once() {
    if (g_setup_called) {
        return;
    }
    g_setup_called = true;
    setup();
}

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

EMSCRIPTEN_KEEPALIVE extern "C" void change_frame_rate(float fps) {
    g_frame_time = 1000 / fps;
    if (g_loop_job_id == -1) {
        // If the loop is not running, do nothing.
        return;
    }
    // stop the loop and start it again with the new frame rate.
    emscripten_clear_interval(g_loop_job_id);
    g_loop_job_id = emscripten_set_interval(interval_loop, g_frame_time, nullptr);
}

EMSCRIPTEN_KEEPALIVE extern "C" void start_loop() {
    // Receives a function to call and some user data to provide it.
    //emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
    if (g_loop_job_id != -1) {
    printf("ignoring call to start_loop, since it was already called.\r\n");
    }
    g_loop_job_id = emscripten_set_interval(interval_loop, g_frame_time, nullptr);
}

EMSCRIPTEN_KEEPALIVE extern "C" void cancel_loop() {
    if (g_loop_job_id == -1) {
        printf("ignoring call to cancel_loop, since it was never called.\r\n");
    }
    emscripten_clear_interval(g_loop_job_id);
    g_loop_job_id = -1;
}


EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    printf("Hello from FastLED - start_loop called\r\n");
    start_loop();
    return 0;
}
