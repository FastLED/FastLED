/// ARM Cortex-M interrupt control implementation

#ifdef FASTLED_ARM

#include "platforms/arm/interrupt.h"

namespace fl {

void noInterrupts() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

void interrupts() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

}  // namespace fl

#endif  // FASTLED_ARM
