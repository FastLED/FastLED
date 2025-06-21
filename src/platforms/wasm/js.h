#pragma once

#include "fl/stdint.h"


// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
uint32_t millis();

// Replacement for 'micros' in WebAssembly context
uint32_t micros();

// Replacement for 'delay' in WebAssembly context
void delay(int ms);
void delayMicroseconds(int micros);
}

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
extern "C" int extern_setup();
extern "C" int extern_loop();
