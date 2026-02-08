// ok no namespace fl
#pragma once

#include "fl/stl/stdint.h"


// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
// NOTE: millis(), micros(), delayMicroseconds() moved to platform_time.cpp.hpp
extern "C" {
// Timer functions declared in timer.cpp.hpp
fl::u32 millis();
fl::u32 micros();
}

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
extern "C" int extern_setup();
extern "C" int extern_loop();
