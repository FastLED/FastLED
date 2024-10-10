#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h> // Include for emscripten_request_animation_frame

#include "FastLED.h"

void setup() {
   printf("FastLED setup ran.\r\n");
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
void emscripten_loop(double time) {
    loop();
    emscripten_request_animation_frame(emscripten_loop);
}

EMSCRIPTEN_KEEPALIVE int main() {
    printf("Hello from FastLED\r\n");
    setup();
    emscripten_request_animation_frame(emscripten_loop);
    return 0;
}
