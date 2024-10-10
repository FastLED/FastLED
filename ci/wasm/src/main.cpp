#include <stdio.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EMSCRIPTEN_KEEPALIVE EXTERN "C" int main() {
   printf("Hello from FastLED\r\n");
   return 0;
}
