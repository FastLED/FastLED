#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

// You can run this in node like the following
//  $ node
//  > const foo = await import('./fastled.js')
//  > foo.default()
//  > console.log(await foo.default()[0])
//  ---> prints out "Hello from FastLED"
//  Or alternatively, you can run this:
//  > let fastled = require('./fastled');
//  > fastled();
EMSCRIPTEN_KEEPALIVE extern "C" int main() {
   printf("Hello from FastLED\r\n");
   return 0;
}
