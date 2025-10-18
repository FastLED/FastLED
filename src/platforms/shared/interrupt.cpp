/// Shared/generic interrupt control implementation for desktop platforms

#include "platforms/shared/interrupt.h"

namespace fl {

void noInterrupts() {
    // No-op: desktop platforms don't have hardware interrupts
}

void interrupts() {
    // No-op: desktop platforms don't have hardware interrupts
}

}  // namespace fl
