/// ESP32-specific interrupt control implementation

#ifdef ESP32

#include "platforms/esp/32/interrupt.h"
#include "freertos/portmacro.h"

namespace fl {

void noInterrupts() {
    portDISABLE_INTERRUPTS();
}

void interrupts() {
    portENABLE_INTERRUPTS();
}

}  // namespace fl

#endif  // ESP32
