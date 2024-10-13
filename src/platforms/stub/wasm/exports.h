#pragma once

// For some reason including the declaration does not work, you must include the *.hpp file??!?!?!

/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup();
EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop();

// Send frame data to the JavaScript side.
void jsOnFrame();
void jsSetCanvasSize(int width, int height);