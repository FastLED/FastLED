#ifndef __INC_FASTLED_STUB_H
#define __INC_FASTLED_STUB_H

// fastpin_stub.h isn't needed (as it's not used by clockless)
#include "fastspi_stub.h"
#include "clockless_stub.h"

/**
 * Could avoid clockless and fastspi with
 * #define NO_HARDWARE_PIN_SUPPORT
 * #undef HAS_HARDWARE_PIN_SUPPORT
 * But then compiling throws pragma message about "Forcing software SPI"
 */
#ifndef HAS_HARDWARE_PIN_SUPPORT
#define HAS_HARDWARE_PIN_SUPPORT
#endif

#endif // __INC_FASTLED_STUB_H