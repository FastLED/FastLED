#pragma once
/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "slice.h"

typedef Slice<uint8_t> SliceUint8;
struct StripData {
    int index = 0;
    SliceUint8 slice;
};

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS

EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup();
EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop();
EMSCRIPTEN_KEEPALIVE extern "C" void async_start_loop();
EMSCRIPTEN_KEEPALIVE extern "C" int main();

// Send frame data to the JavaScript side.
void jsOnFrame(Slice<StripData> data);
void jsSetCanvasSize(int width, int height);