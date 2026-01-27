// ok no namespace fl
#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/function.h"

// Stub timing functions for Arduino compatibility
// These provide timing functionality when using the stub platform
// Only declare these when NOT on a real Arduino platform (exclude real Arduino, not stub Arduino)

#if !defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO)
extern "C" {
    // Global timing functions for Arduino compatibility
    // These are provided by the platform layer but need to be declared globally
    uint32_t millis(void);
    uint32_t micros(void);
    void yield(void);
}

// C++ function to override delay behavior for fast testing
void setDelayFunction(const fl::function<void(uint32_t)>& delayFunc);

// Check if delay override is active (for fast testing)
bool isDelayOverrideActive(void);
#endif  // !ARDUINO || FASTLED_USE_STUB_ARDUINO
