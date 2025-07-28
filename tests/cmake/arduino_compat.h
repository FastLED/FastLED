// arduino_compat.h - Enhanced Arduino compatibility for FastLED example compilation testing
// This provides comprehensive Arduino compatibility for direct .ino compilation with MSVC

#pragma once

#include "platforms/wasm/compiler/Arduino.h"


// Basic Arduino function declarations - .ino files provide these implementations
extern void setup();
extern void loop();

// Note: The existing Arduino.h stub provides most necessary compatibility
// This header adds MSVC-specific fixes and additional definitions needed for .ino compilation

// Provide main function for .ino files that don't have one
// Most Arduino IDE preprocessing adds this automatically
#ifndef ARDUINO_MAIN_DEFINED
#define ARDUINO_MAIN_DEFINED
int main() {
    setup();
    loop();
    return 0;
}
#endif 
