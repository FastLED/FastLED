// ok no namespace fl
#pragma once

#include "fl/stl/stdint.h"


// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
uint32_t millis();

// Replacement for 'micros' in WebAssembly context
uint32_t micros();

// Note: delay() removed - FastLED.h provides via "using fl::delay;"
void delayMicroseconds(int micros);
}

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
extern "C" int extern_setup();
extern "C" int extern_loop();
