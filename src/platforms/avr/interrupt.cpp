/// AVR-specific interrupt control implementation

#ifdef __AVR__

#include <avr/interrupt.h>
#include "platforms/avr/interrupt.h"

namespace fl {

void noInterrupts() {
    cli();
}

void interrupts() {
    sei();
}

}  // namespace fl

#endif  // __AVR__
