#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

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
}  // namespace hack






void setup() {
   printf("FastLED setup ran.\r\n");
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


EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup() {
    setup();
    return 0;
}

EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop() {
    loop();
    return 0;
}

EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    printf("Hello from FastLED - use extern_setup and extern_loop\r\n");
    /*
    setup();
    while(true) {
        loop();
    }
    */
    return 0;
}
