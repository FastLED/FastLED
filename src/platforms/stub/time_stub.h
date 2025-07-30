#pragma once

#include "fl/stdint.h"

// Stub timing functions for Arduino compatibility
// These provide timing functionality when using the stub platform

extern "C" {
    uint32_t millis(void);
    uint32_t micros(void);
    void delay(int ms);
    void delayMicroseconds(int us);
    void yield(void);
}
