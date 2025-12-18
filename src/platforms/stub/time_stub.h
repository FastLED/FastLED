// ok no namespace fl
#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/function.h"

// Stub timing functions for Arduino compatibility
// These provide timing functionality when using the stub platform
// Only declare these when NOT on a real Arduino platform (exclude real Arduino, not stub Arduino)

#if !defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO)
extern "C" {
    uint32_t millis(void);
    uint32_t micros(void);
    void delay(int ms);
    void delayMicroseconds(int us);
    void yield(void);
}

// C++ function to override delay behavior for fast testing
void setDelayFunction(const fl::function<void(uint32_t)>& delayFunc);
#endif  // !ARDUINO || FASTLED_USE_STUB_ARDUINO
