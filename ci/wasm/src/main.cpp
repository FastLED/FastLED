#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

EMSCRIPTEN_KEEPALIVE extern "C" int main() {
   printf("Hello from FastLED\r\n");
   return 0;
}