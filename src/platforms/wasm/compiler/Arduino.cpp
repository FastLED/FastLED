
#if defined(__EMSCRIPTEN__)

// Non-WASM platforms use the generic stub Arduino.h  
#include "platforms/stub/Arduino.h"  // ok include

SerialEmulation Serial;
SerialEmulation Serial1;
SerialEmulation Serial2;
SerialEmulation Serial3;

#endif
