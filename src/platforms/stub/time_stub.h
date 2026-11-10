// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

// Stub timing functions for Arduino compatibility
// These provide timing functionality when using the stub platform
// Only declare these when NOT on a real Arduino platform (exclude real Arduino, not stub Arduino)

#if !defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO)
extern "C" {
    // Global timing functions for Arduino compatibility
    // These are provided by the platform layer but need to be declared globally
    fl::u32 millis(void) FL_NO_EXCEPT;
    fl::u32 micros(void) FL_NO_EXCEPT;
    void yield(void) FL_NO_EXCEPT;
}

// C++ function to override delay behavior for fast testing
void setDelayFunction(const fl::function<void(fl::u32)>& delayFunc) FL_NO_EXCEPT;

// Clear the delay override (must be called before unloading DLLs that set it)
void clearDelayFunction() FL_NO_EXCEPT;

// Check if delay override is active (for fast testing)
bool isDelayOverrideActive(void) FL_NO_EXCEPT;

namespace fl {
namespace stub {

// Observe requested wire delays without depending on host NOP-loop timing.
void setNanosecondDelayCallback(const fl::function<void(fl::u32)>& callback) FL_NO_EXCEPT;
void clearNanosecondDelayCallback() FL_NO_EXCEPT;
void recordNanosecondDelay(fl::u32 ns) FL_NO_EXCEPT;

} // namespace stub
} // namespace fl
#endif  // !ARDUINO || FASTLED_USE_STUB_ARDUINO
