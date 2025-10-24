/// Shared/generic interrupt control implementation for desktop platforms

// Only compile this if no platform-specific implementation exists
#if !defined(__AVR__) && !defined(FASTLED_ARM) && !defined(ESP32) && !defined(__EMSCRIPTEN__)

#include "platforms/shared/interrupt.h"

namespace fl {

void noInterrupts() {
    // No-op: desktop platforms don't have hardware interrupts
}

void interrupts() {
    // No-op: desktop platforms don't have hardware interrupts
}

}  // namespace fl

#endif  // Platform guards
