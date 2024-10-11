#pragma once
/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS

EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup();
EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop();
EMSCRIPTEN_KEEPALIVE extern "C" void async_start_loop();
EMSCRIPTEN_KEEPALIVE extern "C" int main();
