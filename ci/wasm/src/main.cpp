#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>



#include "FastLED.h"

#include <chrono>
#include <thread>

static const auto start_time = std::chrono::system_clock::now();

namespace hack {
uint32_t millis() {
    auto current_time = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
}

uint32_t micros() {
    auto current_time = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time).count();
}

void delay(int ms) {
    std::this_thread::sleep_for (std::chrono::milliseconds(ms));
}

void yield() {
    std::this_thread::yield();
}

bool g_setup_called = false;
}  // namespace hack






void setup() {
   printf("FastLED setup ran.\r\n");
   if (hack::g_setup_called) {
       return;
   }
}

void loop() {
   printf("FastLED loop ran.\r\n");
   hack::delay(1000);
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
    if (hack::g_setup_called) {
        return;
    }
    hack::g_setup_called = true;
    setup();
    return 0;
}

EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup() {
    setup_once();
    return 0;
}

EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop() {
    setup_once();
    loop();
    return 0;
}

// define request animation frame

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData);

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData) {
    extern_loop();
    emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
    return true;
}


EMSCRIPTEN_KEEPALIVE extern "C" void start_loop() {
  // Receives a function to call and some user data to provide it.
  on_request_animation_frame_loop();
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
