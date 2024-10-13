#pragma once

// For some reason including the declaration does not work, you must include the *.hpp file??!?!?!

/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include "channel_data.h"



//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS


EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup();
EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop();
EMSCRIPTEN_KEEPALIVE extern "C" void async_start_loop();

// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

    // Replacement for 'millis' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t millis();

    // Replacement for 'micros' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE uint32_t micros();

    // Replacement for 'delay' in WebAssembly context
    EMSCRIPTEN_KEEPALIVE void delay(int ms);
}

// Send frame data to the JavaScript side.
void jsOnFrame();
void jsSetCanvasSize(int width, int height);