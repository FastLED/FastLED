
#if defined(FASTLED_USE_STUB_ARDUINO) || defined(__EMSCRIPTEN__)
// STUB platform implementation - excluded for WASM builds which provide their own Arduino.cpp

#include "./Arduino.h"  // ok include

SerialEmulation Serial;
SerialEmulation Serial1;
SerialEmulation Serial2;
SerialEmulation Serial3;

#endif
